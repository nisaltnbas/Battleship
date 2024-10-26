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
// the chance to reuse the same points are the 1%
#define REUSE_P_POINTS 100
#define REUSE_C_POINTS 100

struct shot_message {
    int row;
    int column;
    int hit_or_missed; // 1 for hit, 0 for miss
    int game_over; // New flag to indicate game over
};

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

int check_hit(int row, int column, int ships[COUNTofSHIPS][2]) {
    for (int i = 0; i < COUNTofSHIPS; i++) {
        if (ships[i][0] == row && ships[i][1] == column) {
            return 1;
        }
    }
    return 0;
}

int all_ships_hitted(char area[SIZEofGRID][SIZEofGRID]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int k = 0; k < SIZEofGRID; k++) {
            if (area[i][k] == 'P' || area[i][k] == 'C') {
                return 0;
            }
        }
    }
    return 1;
}

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

    // Use 'P' for Player 1's ships and 'C' for Player 2's ships
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

    int last_player1_attack_row = -1;
    int last_player1_attack_column = -1;
    int last_player2_attack_row = -1;
    int last_player2_attack_column = -1;

    while (!shot.game_over) {
        if (pid == 0) { // Player 2 process
            // Decide whether to reuse last attack coordinates
            if (rand() % REUSE_C_POINTS == 0 && last_player2_attack_row != -1) {
                shot.row = last_player2_attack_row;
                shot.column = last_player2_attack_column;
                printf("Player 2 reuses last attack coordinates: (%d, %d)\n", shot.row, shot.column);
            } else {
                get_random_attack(&shot.row, &shot.column, player2_attacks);
                player2_attacks[shot.row][shot.column] = 1;
                printf("Player 2 attacks Player 1 at (%d, %d)\n", shot.row, shot.column);
            }

            int fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            // Store last attack coordinates for Player 2
            last_player2_attack_row = shot.row;
            last_player2_attack_column = shot.column;

            if (shot.game_over) {
                unlink(FIFO_FILE);
                exit(0);
            }

        } else { // Player 1 process
            // Receive attack from Player 2
            int fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            // Process Player 2's attack
            if (check_hit(shot.row, shot.column, player1_ships)) {
                shot.hit_or_missed = 1;
                player1_grid[shot.row][shot.column] = 'X';
                printf("It's a HIT!\n");
            } else {
                shot.hit_or_missed = 0;
                printf("Player 2 Missed!\n");
            }

            printf("Updated Player 1's Grid:\n");
            print_playground(player1_grid);

            // Check if Player 2 wins
            if (all_ships_hitted(player1_grid)) {
                printf("Player 2 is winner!\n");
                shot.game_over = 1;
                fd = open(FIFO_FILE, O_WRONLY);
                write(fd, &shot, sizeof(shot));
                close(fd);
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }

            // Player 1's turn to attack
            if (rand() % REUSE_P_POINTS == 0 && last_player1_attack_row != -1) {
                shot.row = last_player1_attack_row;
                shot.column = last_player1_attack_column;
                printf("Player 1 reuses last attack coordinates: (%d, %d)\n", shot.row, shot.column);
            } else {
                get_random_attack(&shot.row, &shot.column, player1_attacks);
                player1_attacks[shot.row][shot.column] = 1;
                printf("Player 1 attacks Player 2 at (%d, %d)\n", shot.row, shot.column);
            }

            // Process Player 1's attack
            if (check_hit(shot.row, shot.column, player2_ships)) {
                shot.hit_or_missed = 1;
                player2_grid[shot.row][shot.column] = 'X';
                printf("Player 1 HIT!\n");
            } else {
                shot.hit_or_missed = 0;
                printf("Player 1 Missed!\n");
            }

            printf("Updated Player 2's Grid:\n");
            print_playground(player2_grid);

            // Check if Player 1 wins
            if (all_ships_hitted(player2_grid)) {
                printf("Player 1 is winner!\n");
                shot.game_over = 1;
            }

            // Send outcome back to Player 2
            fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            // Store last attack coordinates for Player 1
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
