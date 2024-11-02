#include <stdio.h>
#include <stdlib.h>

int main() {
    int choice;
while (choice > 0){
        printf("Choose the game mode:\n");
        printf("1. Battle Ship game 4x4 mode\n");
        printf("2. Battle Ship game 8x8 mode\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                // Run game_P1.c file
                system("./game_P1");
                break;
            case 2:
                // Run game_P2.c file
                system("./game_P2");  
                break;
            case 3:
                return 0; 
            default:
                printf("Invalid choice. Please try again.\n");
                break; 
        }
    } 

    return 0;
}

