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
#define PARENT_REUSE_CHANCE 3

struct shot_message {
    int row;
    int column;
    int outcome; // 1 for hit, 0 for miss
    int game_over; // New flag to indicate game over
};

void playground(char area[SIZEofGRID][SIZEofGRID], int ships[COUNTofSHIPS][2]) {
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int k = 0; k < SIZEofGRID; k++) {
            area[i][k] = '*';
        }
    }

    int number_ships = 0;
    while (number_ships < COUNTofSHIPS) {
        int row = rand() % SIZEofGRID;
        int column = rand() % SIZEofGRID;
        if (area[row][column] == '*') {
            area[row][column] = 'S';
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
            if (area[i][k] == 'S') {
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
    int parent_ships[COUNTofSHIPS][2];
    int child_ships[COUNTofSHIPS][2];
    char parent_grid[SIZEofGRID][SIZEofGRID];
    char child_grid[SIZEofGRID][SIZEofGRID];
    struct shot_message shot;
    shot.game_over = 0;

    int parent_attacks[SIZEofGRID][SIZEofGRID] = {0};
    int child_attacks[SIZEofGRID][SIZEofGRID] = {0};

    playground(parent_grid, parent_ships);
    playground(child_grid, child_ships);

    printf("Parent's Grid:\n");
    print_playground(parent_grid);
    printf("Child's Grid:\n");
    print_playground(child_grid);

    mkfifo(FIFO_FILE, 0666);
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    int last_parent_attack_row = -1;
    int last_parent_attack_column = -1;

    while (!shot.game_over) {
        if (pid == 0) { // Child process
            // Attack Parent's grid
            get_random_attack(&shot.row, &shot.column, child_attacks);
            child_attacks[shot.row][shot.column] = 1;
            printf("Child attacks Parent at (%d, %d)\n", shot.row, shot.column);

            int fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            printf("Child receives outcome: %s\n", shot.outcome == 1 ? "Hit" : "Miss");

            if (shot.game_over) {
                printf("Game Over!\n");
                unlink(FIFO_FILE);
                exit(0);
            }

        } else { // Parent process
            // Receive attack from Child
            int fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            printf("Parent receives attack from Child at (%d, %d)\n", shot.row, shot.column);

            // Process Child's attack
            if (check_hit(shot.row, shot.column, parent_ships)) {
                shot.outcome = 1;
                parent_grid[shot.row][shot.column] = 'X';
                printf("It's a HIT!\n");
            } else {
                shot.outcome = 0;
                printf("Missed!\n");
            }

            printf("Updated Parent's Grid:\n");
            print_playground(parent_grid);

            // Check if Child wins
            if (all_ships_hitted(parent_grid)) {
                printf("Child wins. All ships of Parent have been destroyed.\n");
                shot.game_over = 1;
                fd = open(FIFO_FILE, O_WRONLY);
                write(fd, &shot, sizeof(shot));
                close(fd);
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }

            // Parent's turn to attack
            if (rand() % PARENT_REUSE_CHANCE == 0 && last_parent_attack_row != -1) {
                shot.row = last_parent_attack_row;
                shot.column = last_parent_attack_column;
                printf("Parent reuses last attack coordinates: (%d, %d)\n", shot.row, shot.column);
            } else {
                get_random_attack(&shot.row, &shot.column, parent_attacks);
                parent_attacks[shot.row][shot.column] = 1;
                printf("Parent attacks Child at (%d, %d)\n", shot.row, shot.column);
            }

            // Process Parent's attack
            if (check_hit(shot.row, shot.column, child_ships)) {
                shot.outcome = 1;
                child_grid[shot.row][shot.column] = 'X';
                printf("Parent HIT!\n");
            } else {
                shot.outcome = 0;
                printf("Parent Missed!\n");
            }

            printf("Updated Child's Grid:\n");
            print_playground(child_grid);

            // Check if Parent wins
            if (all_ships_hitted(child_grid)) {
                printf("Parent wins. All ships of Child have been destroyed.\n");
                shot.game_over = 1;
            }

            // Send outcome back to Child
            fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            // Store last attack coordinates
            last_parent_attack_row = shot.row;
            last_parent_attack_column = shot.column;

            if (shot.game_over) {
                unlink(FIFO_FILE);
                kill(pid, SIGKILL);
                exit(0);
            }
        }
    }

    unlink(FIFO_FILE);
    return 0;
}