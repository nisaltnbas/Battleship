#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SIZEofGRID 8
#define BATTLESHIP_SIZE 4
#define CRUISER_SIZE 3
#define DESTROYER_SIZE 2
#define FIFO_FILE "/tmp/battleship_fifo"
#define SHIPS_NUMBER 5
#define MAX_TARGETS 20

struct shot_message {
    int row;
    int column;
    int hit_or_missed;
    int gameOver;
};


typedef struct {
    int row;
    int col;
    int priority;
} TargetPosition;

typedef struct {
    TargetPosition targets[MAX_TARGETS];
    int target_count;
    int last_hit_row;
    int last_hit_col;
    int hunting_mode;
} AIStrategy;



void initialize_strategy(AIStrategy *ai) {
    ai->target_count = 0;
    ai->hunting_mode = 0;
    ai->last_hit_row = -1;
    ai->last_hit_col = -1;
}

void playground(char grid[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            grid[i][j] = '*';
        }
    }
}

void print_playground(char grid[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            printf("%c ", grid[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

//checking the 1 gap rule
int gap_rule_valid(char grid[SIZEofGRID][SIZEofGRID], int row, int col, int size, int is_vertical) {
    // check if a ship is vertically or horizontally placed
    for (int i = 0; i < size; i++) {
        int r, c;
        if (is_vertical) {
            r = row + i;
            c = col;
        } else {
            r = row;
            c = col + i;
        }
        //check if ship is out the grid or if grid is occupied -- ship can't be placed
        if (r >= SIZEofGRID || c >= SIZEofGRID || grid[r][c] != '*') {
            return 0;
        }

        // check for a 1-cell gaps
        for (int cell_above_or_below = -1; cell_above_or_below <= 1; cell_above_or_below++) { //cell_above_or_below: -1 (bellow) and 1 (above)

            for (int cell_left_or_right = -1; cell_left_or_right <= 1; cell_left_or_right++) {//cell_above_or_below: -1 (left) and 1 (right)
                // check if it is the ship
                if (cell_above_or_below == 0 && cell_left_or_right == 0) {
                    continue;
                }
                //check the cells around a placed ship
                int new_row_cell = r + cell_above_or_below;
                int new_column_cell = c + cell_left_or_right;

                // check if we check only inside the grid
                if (new_row_cell >= 0 && new_row_cell < SIZEofGRID && new_column_cell >= 0 && new_column_cell < SIZEofGRID) {
                    if (grid[new_row_cell][new_column_cell] != '*') {
                        // there is no 1-cell gap, can't place the ship
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}
// place a ship on the grid
void place_ship(char grid[SIZEofGRID][SIZEofGRID], int row, int col, int size, int is_vertical, char ship_char) {
    for (int i = 0; i < size; i++) {
        int r = row + (is_vertical ? i : 0);
        int c = col + (is_vertical ? 0 : i);
        grid[r][c] = ship_char;
    }
}

void place_all_ships(char grid[SIZEofGRID][SIZEofGRID], char ship_char) {
    //how many ships are already placed
    int placed_ships = 0;

    while (placed_ships < SHIPS_NUMBER) {
        //choose random ship's placing points
        int row = rand() % SIZEofGRID;
        int col = rand() % SIZEofGRID;
        //randomly choose the direction: 1 - Vertical, 0 - Horizontal
        int is_vertical = rand() % 2;
        int size;

        if (placed_ships == 0) {
             //placing first ship will be battleship
            size = BATTLESHIP_SIZE;
        } else if (placed_ships < 3) {
            //secondly placed ships will be cruisers
            size = CRUISER_SIZE;
        } else {
            //thirdly placed ships will destroyers
            size = DESTROYER_SIZE;
        }
        //check gaps rule
        if (gap_rule_valid(grid, row, col, size, is_vertical)) {
            place_ship(grid, row, col, size, is_vertical, ship_char);
            placed_ships++;
        }
    }
}

//check if all ships are hit
int all_ships_hitted(char grid[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            if (grid[i][j] == 'P' || grid[i][j] == 'C') {
                return 0;
            }
        }
    }
    return 1;
}

void add_target_positions(AIStrategy *ai, int row, int col, int attacks[SIZEofGRID][SIZEofGRID]) {
    // find moving directions for around cells(up, right, down, left)
    int directions[4][2] = {{-1,0}, {0,1}, {1,0}, {0,-1}};
    
    // Loop through each direction to add surrounding cells as target positions
    for (int i = 0; i < 4; i++) {
        int new_row = row + directions[i][0];
        int new_col = col + directions[i][1];
        
        // Check if the new position is within grid bounds and not already attacked
        if (new_row >= 0 && new_row < SIZEofGRID && 
            new_col >= 0 && new_col < SIZEofGRID && 
            !attacks[new_row][new_col]) {
            
            // Add the new position as a target if there's room in the target list
            if (ai->target_count < MAX_TARGETS) {
                ai->targets[ai->target_count].row = new_row;
                ai->targets[ai->target_count].col = new_col;
                ai->targets[ai->target_count].priority = 2; // Set priority for strategic targeting
                ai->target_count++;
            }
        }
    }
}

void execute_smart_attack(int *row, int *col, int attacks[SIZEofGRID][SIZEofGRID], AIStrategy *ai) {
    // if AI is in hunting mode and has targets available
    if (ai->hunting_mode && ai->target_count > 0) {
        // Find the target with the highest priority
        int best_index = 0;
        int highest_priority = ai->targets[0].priority;
        
        // iterate through targets to select the one with the highest priority
        for (int i = 1; i < ai->target_count; i++) {
            if (ai->targets[i].priority > highest_priority) {
                highest_priority = ai->targets[i].priority;
                best_index = i;
            }
        }
        
        // assign selected target's coordinates for the attack
        *row = ai->targets[best_index].row;
        *col = ai->targets[best_index].col;
        
        // remove the used target from the target list by shifting remaining targets left
        for (int i = best_index; i < ai->target_count - 1; i++) {
            ai->targets[i] = ai->targets[i + 1];
        }
        ai->target_count--; // Decrement target count after removal
    } else {
        // else if no targets, perform a random attack
        do {
            *row = rand() % SIZEofGRID;
            *col = rand() % SIZEofGRID;
        } while (attacks[*row][*col]); 
    }
}

//check if a shot hits a ship
int check_hit(char grid[SIZEofGRID][SIZEofGRID], int row, int col) {
    return (grid[row][col] == 'P' || grid[row][col] == 'C');
}

int main() {
    srand(time(NULL));

    char player1_grid[SIZEofGRID][SIZEofGRID];
    char player2_grid[SIZEofGRID][SIZEofGRID];
    struct shot_message p1_shot, p2_shot;  // Separate shot structs for each player
    p1_shot.gameOver = p2_shot.gameOver = 0;

    int player1_attacks[SIZEofGRID][SIZEofGRID] = {0};
    int player2_attacks[SIZEofGRID][SIZEofGRID] = {0};

    AIStrategy player1_ai, player2_ai;
    initialize_strategy(&player1_ai);
    initialize_strategy(&player2_ai);

    playground(player1_grid);
    playground(player2_grid);

    printf("Placing Player 1's Grid:\n");
    place_all_ships(player1_grid, 'P');
    print_playground(player1_grid);

    printf("Placing Player 2's Grid:\n");
    place_all_ships(player2_grid, 'C');
    print_playground(player2_grid);

    mkfifo(FIFO_FILE, 0666);
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    while (!p1_shot.gameOver && !p2_shot.gameOver) {
        if (pid == 0) {  
            // player 2 attack
            execute_smart_attack(&p2_shot.row, &p2_shot.column, player2_attacks, &player2_ai);
            player2_attacks[p2_shot.row][p2_shot.column] = 1;
            printf("Player 2 attacks Player 1 at (%d, %d)\n", p2_shot.row, p2_shot.column);

            int fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &p2_shot, sizeof(p2_shot));
            close(fd);

            fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &p2_shot, sizeof(p2_shot));
            close(fd);

            if (p2_shot.hit_or_missed) {
                player2_ai.hunting_mode = 1;
                player2_ai.last_hit_row = p2_shot.row;
                player2_ai.last_hit_col = p2_shot.column;
                add_target_positions(&player2_ai, p2_shot.row, p2_shot.column, player2_attacks);
            }

            if (p2_shot.gameOver) {
                unlink(FIFO_FILE);
                exit(0);
            }

        } else { 
            // player1 handle player2's shot
            int fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &p2_shot, sizeof(p2_shot));
            close(fd);

            if (check_hit(player1_grid, p2_shot.row, p2_shot.column)) {
                p2_shot.hit_or_missed = 1;
                player1_grid[p2_shot.row][p2_shot.column] = 'X';
                printf("Player 2 HITS!\n");
            } else {
                p2_shot.hit_or_missed = 0;
                printf("Player 2 Missed!\n");
            }
            printf("Updated Player 1's Grid:\n");
            print_playground(player1_grid);

            if (all_ships_hitted(player1_grid)) {
                printf("Player 2 wins!\n");
                p2_shot.gameOver = 1;
                fd = open(FIFO_FILE, O_WRONLY);
                write(fd, &p2_shot, sizeof(p2_shot));
                close(fd);
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }

            // player1 attack
            execute_smart_attack(&p1_shot.row, &p1_shot.column, player1_attacks, &player1_ai);
            player1_attacks[p1_shot.row][p1_shot.column] = 1;
            printf("Player 1 attacks Player 2 at (%d, %d)\n", p1_shot.row, p1_shot.column);

            if (check_hit(player2_grid, p1_shot.row, p1_shot.column)) {
                p1_shot.hit_or_missed = 1;
                player2_grid[p1_shot.row][p1_shot.column] = 'X';
                printf("Player 1 HITS!\n");
                
                player1_ai.hunting_mode = 1;
                player1_ai.last_hit_row = p1_shot.row;
                player1_ai.last_hit_col = p1_shot.column;
                add_target_positions(&player1_ai, p1_shot.row, p1_shot.column, player1_attacks);
            } else {
                p1_shot.hit_or_missed = 0;
                printf("Player 1 Missed!\n");
            }

            printf("Updated Player 2's Grid:\n");
            print_playground(player2_grid);

            if (all_ships_hitted(player2_grid)) {
                printf("Player 1 wins!\n");
                p1_shot.gameOver = 1;
                  fd = open(FIFO_FILE, O_WRONLY);
                write(fd, &p1_shot, sizeof(p1_shot));
                close(fd);
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }

            fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &p2_shot, sizeof(p2_shot));
            close(fd);

            if (p2_shot.gameOver || p1_shot.gameOver) {
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }
        }
    }
    return 0;}
