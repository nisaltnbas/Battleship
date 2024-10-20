#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define SIZEofGRID 4
#define COUNTofSHIPS 4

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

int main()
{
    srand(time(NULL)); // Random number generation

    int parent_ships[COUNTofSHIPS][2];
    int child_ships[COUNTofSHIPS][2];

    char parent_grid[SIZEofGRID][SIZEofGRID];
    char child_grid[SIZEofGRID][SIZEofGRID];

    playground(parent_grid, parent_ships); // Create grid for parent
    playground(child_grid, child_ships);   // Create grid for child

    printf("Parent's Grid:\n");
    print_playground(parent_grid);
    printf("Child's Grid:\n");
    print_playground(child_grid);

    // Child attacks parent
    struct shot_message shot;
    shot.row = rand() % SIZEofGRID;
    shot.column = rand() % SIZEofGRID;

    printf("Child attacks Parent at (%d, %d)\n", shot.row, shot.column);

    if (check_hit(shot.row, shot.column, parent_ships))
    {
        strcpy(shot.outcome, "Hit");
        parent_grid[shot.row][shot.column] = 'X'; // Mark hit
        printf("It's a HIT!\n");
    }
    else
    {
        strcpy(shot.outcome, "Miss");
        printf("Missed!\n");
    }

    printf("\nUpdated Parent's Grid:\n");
    print_playground(parent_grid);

    if (all_ships_hitted(parent_grid))
    {
        printf("Child wins.\n");
    }
    else
    {
        printf("Parent still has ships remaining.\n");
    }
    printf("----------------------\n");

    // Parent attacks child
    struct shot_message shot_parent;
    shot_parent.row = rand() % SIZEofGRID;
    shot_parent.column = rand() % SIZEofGRID;

    printf("Parent attacks Child at (%d, %d)\n", shot_parent.row, shot_parent.column);

    if (check_hit(shot_parent.row, shot_parent.column, child_ships))
    {
        strcpy(shot_parent.outcome, "Hit");
        child_grid[shot_parent.row][shot_parent.column] = 'X'; // Mark hit
        printf("It's a HIT!\n");
    }
    else
    {
        strcpy(shot_parent.outcome, "Miss");
        printf("Missed!\n");
    }

    printf("\nUpdated Child's Grid:\n");
    print_playground(child_grid);

    if (all_ships_hitted(child_grid))
    {
        printf("Parent wins.\n");
    }
    else
    {
        printf("Child still has ships remaining.\n");
    }

    return 0;
}
