#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


#define SIZEofGRID 4
#define COUNTofSHIPS 4

#define FIFO_FILE "/tmp/battleship_fifo"

struct shot_message
{
    int row;
    int column;
    char outcome[9]; // hit or miss
};

void playground(char area[SIZEofGRID][SIZEofGRID], int ships[COUNTofSHIPS][2])
{
    for (int i = 0; i < SIZEofGRID; i++)
    {
        for (int k = 0; k < SIZEofGRID; k++)
        {
            area[i][k] = '*';
        }
    } // explaining the play ground for the battleship. ground is 4*4 and we put the '*' sign for every cell.

    int number_ships = 0; // it is a counter of the number of the ships for every player.
    while (number_ships < COUNTofSHIPS)
    {
        int row = rand() % SIZEofGRID;    // x
        int column = rand() % SIZEofGRID; // y
        if (area[row][column] == '*')
        {
            area[row][column] = 'S';
            ships[number_ships][0] = row;
            ships[number_ships][1] = column;
            number_ships++;
        }
    }
} // In this function, we defined the battle ground and created the ships (totally 8 ships).in the for loop, we selected the cell randomly for row and column.
  // with the area parameter, we controlled if there were any empty cells.if the empty, we put the ship(G) and store the row and column.

void print_playground(char area[SIZEofGRID][SIZEofGRID])
{
    for (int i = 0; i < SIZEofGRID; i++)
    {
        for (int k = 0; k < SIZEofGRID; k++)
        {
            printf("%c ", area[i][k]);
        }
        printf("\n");
    }
    printf("\n");
} // because of this function, after the create the area we can print it.

int check_hit(int row, int column, int ships[COUNTofSHIPS][2])
{
    for (int i = 0; i < COUNTofSHIPS; i++)
    {
        if (ships[i][0] == row && ships[i][1] == column)
        {
            return 1; // Hit
        }
    }
    return 0; // Miss
}

int all_ships_hitted(char area[SIZEofGRID][SIZEofGRID])
{
    for (int i = 0; i < SIZEofGRID; i++)
    {
        for (int k = 0; k < SIZEofGRID; k++)
        {
            if (area[i][k] == 'S')
            {
                return 0; // Not all ships destroyed
            }
        }
    }
    return 1; // All ships destroyed
}


int main() {
    srand(time(NULL));
    int parent_ships[COUNTofSHIPS][2];
    int child_ships[COUNTofSHIPS][2];
    char parent_grid[SIZEofGRID][SIZEofGRID];
    char child_grid[SIZEofGRID][SIZEofGRID];
    struct shot_message shot;

    // Create grids and ships for both parent and child
    playground(parent_grid, parent_ships);
    playground(child_grid, child_ships);

    // Print the grids
    printf("Parent's Grid:\n");
    print_playground(parent_grid);
    printf("Child's Grid:\n");
    print_playground(child_grid);

    // Create FIFO
    mkfifo(FIFO_FILE, 0666);

    // Create child process
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    while (1) {
        if (pid == 0) { // Child process
            // Child attacks parent
            shot.row = rand() % SIZEofGRID;
            shot.column = rand() % SIZEofGRID;
            printf("Child attacks Parent at (%d, %d)\n", shot.row, shot.column);

            // Open FIFO for writing
            int fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);

            // Now, the child waits for the result from the parent
            fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            printf("Child receives outcome: %s\n", shot.outcome);
        } else { // Parent process
            // Parent reads the shot from the child via the pipe
            int fd = open(FIFO_FILE, O_RDONLY);
            read(fd, &shot, sizeof(shot));
            close(fd);

            printf("Parent receives attack from Child at (%d, %d)\n", shot.row, shot.column);

            
            if (check_hit(shot.row, shot.column, parent_ships)) {
                strcpy(shot.outcome, "Hit");
                parent_grid[shot.row][shot.column] = 'X'; // Mark hit
                printf("It's a HIT!\n");
            } else {
                strcpy(shot.outcome, "Miss");
                printf("Missed!\n");
            }

          
            printf("Updated Parent's Grid:\n");
            print_playground(parent_grid);

           
            if (all_ships_hitted(parent_grid)) {
                printf("Child wins. All ships of Parent have been destroyed.\n");
                unlink(FIFO_FILE);
                exit(0);
            }

           
            shot.row = rand() % SIZEofGRID;
            shot.column = rand() % SIZEofGRID;
            printf("Parent attacks Child at (%d, %d)\n", shot.row, shot.column);

            // Check hit or miss for the parent's attack
            if (check_hit(shot.row, shot.column, child_ships)) {
                strcpy(shot.outcome, "Hit");
                child_grid[shot.row][shot.column] = 'X'; // Mark hit
                printf("Parent HIT!\n");
            } else {
                strcpy(shot.outcome, "Miss");
                printf("Parent Missed!\n");
            }

            
            printf("Updated Child's Grid:\n");
            print_playground(child_grid);

            // Send outcome back to the child
            fd = open(FIFO_FILE, O_WRONLY);
            write(fd, &shot, sizeof(shot));
            close(fd);
              printf("Parent receives outcome: %s\n", shot.outcome);

            
            if (all_ships_hitted(child_grid)) {
                printf("Parent wins. All ships of Child have been destroyed.\n");
                unlink(FIFO_FILE);
                exit(0);
            }
        }
    }

    // Clean up the FIFO
    unlink(FIFO_FILE);
    return 0;
}

