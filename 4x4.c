#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SIZEofGRID 4
#define COUNTofSHIPS 4
#define FIFO_FILE "/tmp/battleship_fifo"


struct shot_message {
    int row;
    int column;
    // 1 for hit, 0 for miss
    int hit_or_missed;
    //flag for game over
    int game_over; 
};

//build a playground with rondom ships 
void playground(char area[SIZEofGRID][SIZEofGRID], int ships[COUNTofSHIPS][2], char ship_char) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            area[i][j] = '*';
        }
    }
    int number_ships = 0;
    while (number_ships < COUNTofSHIPS) {
        int row = rand() % SIZEofGRID;
        int column = rand() % SIZEofGRID;
        if (area[row][column] == '*') {
            area[row][column] = ship_char;
            ships[number_ships][0] = row;
            ships[number_ships][1] = column;
            number_ships++;
        }
    }
}

void print_playground(char area[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int k = 0; k < SIZEofGRID; k++) {
            printf("%c ", area[i][k]);
        }
        printf("\n");
    }
    printf("\n");
}

// check hit function
int check_hit(int row, int column, int ships[COUNTofSHIPS][2]) {
    for (int i = 0; i < COUNTofSHIPS; i++) {
        if (ships[i][0] == row && ships[i][1] == column) {
            //it is a hit
            return 1;
        }
    }
    return 0;
}


int all_P_ships_hitted(char area[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            if (area[i][j] == 'P') {
                return 0;
            }
        }
    }
    // all hitted
    return 1;
}

int all_C_ships_hitted(char area[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            if (area[i][j] == 'C') {
                return 0;
            }
        }
    }
    // all hitted
    return 1;
}

//randomize attack point
void get_random_attack(int *row, int *column, int attacks[SIZEofGRID][SIZEofGRID]) {
    do {
        *row = rand() % SIZEofGRID;
        *column = rand() % SIZEofGRID;
    } while (attacks[*row][*column]);
}

int main() {
    srand(time(NULL));
    int player1_ships[COUNTofSHIPS][2];
    int player2_ships[COUNTofSHIPS][2];
    char player1_grid[SIZEofGRID][SIZEofGRID];
    char player2_grid[SIZEofGRID][SIZEofGRID];
    struct shot_message shot;
    shot.game_over = 0;

    int player1_attacks[SIZEofGRID][SIZEofGRID] = {0};
    int player2_attacks[SIZEofGRID][SIZEofGRID] = {0};

    //'P' ship of player1 and 'C' ship of player2
    playground(player1_grid, player1_ships, 'P');
    playground(player2_grid, player2_ships, 'C');

    printf("Player 1's Grid:\n");
    print_playground(player1_grid);
    printf("Player 2's Grid:\n");
    print_playground(player2_grid);

    mkfifo(FIFO_FILE, 0666);
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
  
    //if no attacks had been done
    int last_player1_attack_row = -1;
    int last_player1_attack_column = -1;
    int last_player2_attack_row = -1;
    int last_player2_attack_column = -1;


    while (!shot.game_over) {
        if (pid == 0) { 
               // player2 play
               // do not repeat the previous attacks
               if (last_player2_attack_row != -1){
                do{
                    get_random_attack(&shot.row, &shot.column, player2_attacks);
                    
                }while(shot.row == last_player2_attack_row && shot.column == last_player2_attack_column);
               }
               else{
                  get_random_attack(&shot.row, &shot.column, player2_attacks); 
               }
                
               player2_attacks[shot.row][shot.column] = 1;
               printf("Player 2 attacks at (%d, %d)\n", shot.row, shot.column);
            
            
            int fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            //save current points as last attack for player2
            last_player2_attack_row = shot.row;
            last_player2_attack_column = shot.column;

            if (shot.game_over) {
                unlink(FIFO_FILE);
                exit(0);
            }

        } 
            // player1 play
            else{
            //get attack from player2
            int fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            //if player2 hitted aor missed
            if (check_hit(shot.row, shot.column, player1_ships)) {
                shot.hit_or_missed = 1;
                player1_grid[shot.row][shot.column] = 'X';
                printf("It's a HIT!\n");
                // if it is a hit -- show new grid of player1
                printf("Updated Player 1's Grid:\n");
                print_playground(player1_grid);
            } 
            else {
                shot.hit_or_missed = 0;
                printf("Player 2 Missed!\n");
            }

            //check if player2 wins
            if (all_P_ships_hitted(player1_grid)) {
                printf("Player 2 is winner!\n");
                //change the flaf to end the game
                shot.game_over = 1;
                
                //write for parent process that game is over 
                fd = open(FIFO_FILE, O_WRONLY);
                write(fd, &shot, sizeof(shot));
                close(fd);
                //unlink the fifo and end the child process to run
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }

            // player1's turn to play
            if (last_player1_attack_row != -1){
                // do not repeat the previous attacks
                do{
                    get_random_attack(&shot.row, &shot.column, player1_attacks);
                }while(shot.row == last_player1_attack_row && shot.column == last_player1_attack_column);
            }
            else{
                  get_random_attack(&shot.row, &shot.column, player1_attacks); 
                }
                player1_attacks[shot.row][shot.column] = 1;
                printf("Player 1 attacks at (%d, %d)\n", shot.row, shot.column);
            
            // player1 attack
            if (check_hit(shot.row, shot.column, player2_ships)) {
                shot.hit_or_missed = 1;
                player2_grid[shot.row][shot.column] = 'X';
                printf("Player 1 HIT!\n");
                printf("Updated Player 2's Grid:\n");
               print_playground(player2_grid);
            } 
            else {
                shot.hit_or_missed = 0;
                printf("Player 1 Missed!\n");
            }

            //check if player1 wins
            if (all_C_ships_hitted(player2_grid)) {
                printf("Player 1 is winner!\n");
                shot.game_over = 1;
            }

            //send outcome to player2
            fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            //save current point as last attack point
            last_player1_attack_row = shot.row;
            last_player1_attack_column = shot.column;

            if (shot.game_over) {
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }
        }
    }
}