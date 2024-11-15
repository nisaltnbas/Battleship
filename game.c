
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <signal.h>

// Game constants
#define SIZEofGRID 8
#define BATTLESHIP_SIZE 4
#define CRUISER_SIZE 3
#define DESTROYER_SIZE 2
#define FIFO_FILE "/tmp/battleship_fifo"
#define SHIPS_NUMBER 5
#define MAX_TARGETS 20


WINDOW *grid1_win;
WINDOW *grid2_win;
WINDOW *log_win;

// Layout values
#define GRID_HEIGHT 12
#define GRID_WIDTH 28  
#define LOG_HEIGHT 12                 
#define LOG_WIDTH 60 


#define MAX_LOG_MESSAGES 8
#define MENU_OFFSET_X 35
#define MENU_OFFSET_Y 10

// Color pairs definitions
#define WATER_PAIR 1
#define SHIP_PAIR 2
#define HIT_PAIR 3
#define MISS_PAIR 4
#define TEXT_PAIR 5
#define MENU_PAIR 6
#define SELECTED_PAIR 7
#define BORDER_PAIR 8
#define TITLE_PAIR 9


#define MSG_BOX_HEIGHT 3
#define MSG_BOX_WIDTH 40

// Windows for status message
WINDOW *msg_win = NULL;

// Game enums
typedef enum {
    MENU_NEW_GAME,
    MENU_RULES,
    MENU_EXIT,
    MENU_ITEMS_COUNT
} MainMenuItem;

typedef enum {
    GAME_MENU_CONTINUE,
    GAME_MENU_RELOCATE,
    GAME_MENU_TOGGLE_VIEW,
    EXIT,
    GAME_MENU_ITEMS_COUNT
} GameMenuItem;

// Game structures
struct shot_message {
    int row;
    int col;
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

typedef struct {
    char messages[MAX_LOG_MESSAGES][100];
    int count;
} GameLog;

// Global variables
int show_grid = 1;
int in_game = 0;

// Function declarations
void initialize_strategy(AIStrategy *ai);
void playground(char grid[SIZEofGRID][SIZEofGRID]);
int gap_rule_valid(char grid[SIZEofGRID][SIZEofGRID], int row, int col, int size, int is_vertical);
void place_ship(char grid[SIZEofGRID][SIZEofGRID], int row, int col, int size, int is_vertical, char ship_char);
void place_all_ships(char grid[SIZEofGRID][SIZEofGRID], char ship_char);
int check_hit(char grid[SIZEofGRID][SIZEofGRID], int row, int col);
void execute_smart_attack(int *row, int *col, int attacks[SIZEofGRID][SIZEofGRID], AIStrategy *ai);
void add_target_positions(AIStrategy *ai, int row, int col, int attacks[SIZEofGRID][SIZEofGRID]);
void init_game_log(GameLog *log);
void update_game_log(GameLog *log, const char *message);
void display_game_log(GameLog *log);
void setup_colors(void);
int display_main_menu(void);
void display_rules(void);
int display_game_menu(void);
void draw_grid(WINDOW *win, char grid[SIZEofGRID][SIZEofGRID], const char *title);
void draw_box(int y, int x, int height, int width);
void init_windows();
void update_display(char grid1[SIZEofGRID][SIZEofGRID], char grid2[SIZEofGRID][SIZEofGRID]);
void cleanup_windows();




void show_status_message(const char* message, int delay);
void process_move(char grid[SIZEofGRID][SIZEofGRID], struct shot_message *shot, 
                 const char *player_name, AIStrategy *ai, int attacks[SIZEofGRID][SIZEofGRID]);
int check_game_over(char grid[SIZEofGRID][SIZEofGRID], const char *player_name);                 



#define MOVE_DELAY 100   
#define END_GAME_DELAY 1000 

void show_status_message(const char* message, int delay) {
    if (msg_win != NULL) {
        delwin(msg_win);
    }
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Place message box
    int start_y = max_y - MSG_BOX_HEIGHT - 1;
    int start_x = (max_x - MSG_BOX_WIDTH) / 2;
    
    msg_win = newwin(MSG_BOX_HEIGHT, MSG_BOX_WIDTH, start_y, start_x);
    box(msg_win, 0, 0);
    
    
    int msg_len = strlen(message);
    int msg_x = (MSG_BOX_WIDTH - msg_len) / 2;
    
    wattron(msg_win, COLOR_PAIR(TEXT_PAIR) | A_BOLD);
    mvwprintw(msg_win, 1, msg_x, "%s", message);
    wattroff(msg_win, COLOR_PAIR(TEXT_PAIR) | A_BOLD);
    
    wrefresh(msg_win);
    refresh();
    
    napms(delay);
}



// Update the move operations in the main game loop
void process_move(char grid[SIZEofGRID][SIZEofGRID], struct shot_message *shot, 
                 const char *player_name, AIStrategy *ai, int attacks[SIZEofGRID][SIZEofGRID]) {
    
    char message[100];
    
    // Message before attack
    snprintf(message, sizeof(message), "%s attacking...", player_name);
    show_status_message(message, MOVE_DELAY / 2);
    
    //Attack
    execute_smart_attack(&shot->row, &shot->col, attacks, ai);
    attacks[shot->row][shot->col] = 1;
    
    // Control and show result
    if (check_hit(grid, shot->row, shot->col)) {
        grid[shot->row][shot->col] = 'X';
        shot->hit_or_missed = 1;
        
        snprintf(message, sizeof(message), "%s HIT at %c%d!", 
                player_name, 'A' + shot->row, shot->col);
        show_status_message(message, MOVE_DELAY);
        
        // AI strategy updating
        ai->hunting_mode = 1;
        ai->last_hit_row = shot->row;
        ai->last_hit_col = shot->col;
        add_target_positions(ai, shot->row, shot->col, attacks);
    } else {
        grid[shot->row][shot->col] = 'M';
        shot->hit_or_missed = 0;
        
        snprintf(message, sizeof(message), "%s MISSED at %c%d", 
                player_name, 'A' + shot->row, shot->col);
        show_status_message(message, MOVE_DELAY / 2);
    }
}

// Endgame check and display
int check_game_over(char grid[SIZEofGRID][SIZEofGRID], const char* winner) {
    // Control ships
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            if (grid[i][j] == 'P' || grid[i][j] == 'C') {
                return 0;
            }
        }
    }
    
    // Screen for winning!
    clear();
    refresh();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    char victory_msg[100];
    snprintf(victory_msg, sizeof(victory_msg), "*** %s WINS THE GAME! ***", winner);
    
    
    int box_width = strlen(victory_msg) + 8;
    int box_height = 5;
    int box_y = (max_y - box_height) / 2;
    int box_x = (max_x - box_width) / 2;
    
    WINDOW *victory_win = newwin(box_height, box_width, box_y, box_x);
    box(victory_win, 0, 0);
    
    // Print message
    wattron(victory_win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    mvwprintw(victory_win, 2, 4, "%s", victory_msg);
    wattroff(victory_win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    
    wrefresh(victory_win);
    refresh();
    
    napms(END_GAME_DELAY);
    
    delwin(victory_win);
    return 1;
}


//Update and display
void update_display(char grid1[SIZEofGRID][SIZEofGRID], char grid2[SIZEofGRID][SIZEofGRID]) {
    if (show_grid) {
        draw_grid(grid1_win, grid1, "PLAYER 1 GRID");
        draw_grid(grid2_win, grid2, "PLAYER 2 GRID");
    }
    
    
    attron(COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    mvprintw(0, COLS/2 - 10, "BATTLESHIP GAME");
    attroff(COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    
    
    attron(COLOR_PAIR(TEXT_PAIR));
    mvprintw(LINES-1, 2, "Press ESC for menu");
    attroff(COLOR_PAIR(TEXT_PAIR));
    
    wrefresh(grid1_win);
    wrefresh(grid2_win);
    refresh();
}

void cleanup_windows() {
    if (grid1_win != NULL) delwin(grid1_win);
    if (grid2_win != NULL) delwin(grid2_win);
    if (msg_win != NULL) delwin(msg_win);
}
void init_windows() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Position the grids at the top
    int grid_start_y = 2;
    int grid1_start_x = (max_x / 2) - GRID_WIDTH - 2;
    int grid2_start_x = (max_x / 2) + 2;
    
    grid1_win = newwin(GRID_HEIGHT, GRID_WIDTH, grid_start_y, grid1_start_x);
    grid2_win = newwin(GRID_HEIGHT, GRID_WIDTH, grid_start_y, grid2_start_x);
    
    
    keypad(stdscr, TRUE);
    keypad(grid1_win, TRUE);
    keypad(grid2_win, TRUE);
}


void draw_box(int y, int x, int height, int width) {
    attron(COLOR_PAIR(BORDER_PAIR) | A_BOLD);
    
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + width - 1, ACS_URCORNER);
    mvaddch(y + height - 1, x, ACS_LLCORNER);
    mvaddch(y + height - 1, x + width - 1, ACS_LRCORNER);
    
    for (int i = 1; i < width - 1; i++) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + height - 1, x + i, ACS_HLINE);
    }
    
    for (int i = 1; i < height - 1; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + width - 1, ACS_VLINE);
    }
    
    attroff(COLOR_PAIR(BORDER_PAIR) | A_BOLD);
}

void setup_colors(void) {
    start_color();
    init_pair(WATER_PAIR, COLOR_BLUE, COLOR_BLACK);
    init_pair(SHIP_PAIR, COLOR_GREEN, COLOR_BLACK);
    init_pair(HIT_PAIR, COLOR_RED, COLOR_BLACK);
    init_pair(MISS_PAIR, COLOR_WHITE, COLOR_BLACK);
    init_pair(TEXT_PAIR, COLOR_YELLOW, COLOR_BLACK);
    init_pair(MENU_PAIR, COLOR_CYAN, COLOR_BLACK);
    init_pair(SELECTED_PAIR, COLOR_BLACK, COLOR_CYAN);
    init_pair(BORDER_PAIR, COLOR_WHITE, COLOR_BLACK);
    init_pair(TITLE_PAIR, COLOR_MAGENTA, COLOR_BLACK);
}

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

int gap_rule_valid(char grid[SIZEofGRID][SIZEofGRID], int row, int col, int size, int is_vertical) {
    for (int i = 0; i < size; i++) {
        int r = row + (is_vertical ? i : 0);
        int c = col + (is_vertical ? 0 : i);
        
        if (r >= SIZEofGRID || c >= SIZEofGRID || grid[r][c] != '*') {
            return 0;
        }

        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int nr = r + dr;
                int nc = c + dc;
                if (nr >= 0 && nr < SIZEofGRID && nc >= 0 && nc < SIZEofGRID) {
                    if (grid[nr][nc] != '*') return 0;
                }
            }
        }
    }
    return 1;
}

void place_ship(char grid[SIZEofGRID][SIZEofGRID], int row, int col, int size, int is_vertical, char ship_char) {
    for (int i = 0; i < size; i++) {
        int r = row + (is_vertical ? i : 0);
        int c = col + (is_vertical ? 0 : i);
        grid[r][c] = ship_char;
    }
}

void place_all_ships(char grid[SIZEofGRID][SIZEofGRID], char ship_char) {
    int placed_ships = 0;
    while (placed_ships < SHIPS_NUMBER) {
        int row = rand() % SIZEofGRID;
        int col = rand() % SIZEofGRID;
        int is_vertical = rand() % 2;
        int size;

        if (placed_ships == 0) {
            size = BATTLESHIP_SIZE;
        } else if (placed_ships < 3) {
            size = CRUISER_SIZE;
        } else {
            size = DESTROYER_SIZE;
        }

        if (gap_rule_valid(grid, row, col, size, is_vertical)) {
            place_ship(grid, row, col, size, is_vertical, ship_char);
            placed_ships++;
        }
    }
}

int check_hit(char grid[SIZEofGRID][SIZEofGRID], int row, int col) {
    return (grid[row][col] == 'P' || grid[row][col] == 'C');
}

void add_target_positions(AIStrategy *ai, int row, int col, int attacks[SIZEofGRID][SIZEofGRID]) {
    int directions[4][2] = {{-1,0}, {0,1}, {1,0}, {0,-1}};
    
    for (int i = 0; i < 4; i++) {
        int new_row = row + directions[i][0];
        int new_col = col + directions[i][1];
        
        if (new_row >= 0 && new_row < SIZEofGRID && 
            new_col >= 0 && new_col < SIZEofGRID && 
            !attacks[new_row][new_col]) {
            
            if (ai->target_count < MAX_TARGETS) {
                ai->targets[ai->target_count].row = new_row;
                ai->targets[ai->target_count].col = new_col;
                ai->targets[ai->target_count].priority = 2;
                ai->target_count++;
            }
        }
    }
}

void execute_smart_attack(int *row, int *col, int attacks[SIZEofGRID][SIZEofGRID], AIStrategy *ai) {
    if (ai->hunting_mode && ai->target_count > 0) {
        int best_index = 0;
        int highest_priority = ai->targets[0].priority;
        
        for (int i = 1; i < ai->target_count; i++) {
            if (ai->targets[i].priority > highest_priority) {
                highest_priority = ai->targets[i].priority;
                best_index = i;
            }
        }
        
        *row = ai->targets[best_index].row;
        *col = ai->targets[best_index].col;
        
        for (int i = best_index; i < ai->target_count - 1; i++) {
            ai->targets[i] = ai->targets[i + 1];
        }
        ai->target_count--;
    } else {
        do {
            *row = rand() % SIZEofGRID;
            *col = rand() % SIZEofGRID;
        } while (attacks[*row][*col]);
    }
}

void draw_grid(WINDOW *win, char grid[SIZEofGRID][SIZEofGRID], const char *title) {
    wclear(win);
    box(win, 0, 0);
    
    // Grid title
    wattron(win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    mvwprintw(win, 0, (GRID_WIDTH - strlen(title)) / 2 - 1, "%s", title);  // -1 ekleyerek başlığı hafif sola kaydırıyoruz
    wattroff(win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    
    // Column titles
    wattron(win, COLOR_PAIR(TEXT_PAIR));
    for (int j = 0; j < SIZEofGRID; j++) {
        mvwprintw(win, 1, j * 3 + 3, "%d", j + 1);
    }
    
    // Row titles
    for (int i = 0; i < SIZEofGRID; i++) {
         mvwprintw(win, i + 2, 1, "%c", 'A' + i);
    }
    wattroff(win, COLOR_PAIR(TEXT_PAIR));
    
    // Grid content
    for (int i = 0; i < SIZEofGRID; i++) {
        for (int j = 0; j < SIZEofGRID; j++) {
            int color_pair;
            char display_char;
            
            switch (grid[i][j]) {
                case '*': 
                    color_pair = WATER_PAIR; 
                    display_char = '~'; 
                    break;
                case 'P':
                case 'C': 
                    color_pair = SHIP_PAIR; 
                    display_char = '#'; 
                    break;
                case 'X': 
                    color_pair = HIT_PAIR; 
                    display_char = 'X'; 
                    break;
                case 'M': 
                    color_pair = MISS_PAIR; 
                    display_char = 'O'; 
                    break;
                default: 
                    color_pair = WATER_PAIR; 
                    display_char = grid[i][j];
            }
            
            wattron(win, COLOR_PAIR(color_pair) | A_BOLD);
            mvwprintw(win, i + 2, j * 3 + 3, "%c", display_char);
            wattroff(win, COLOR_PAIR(color_pair) | A_BOLD);
        }
    }
    
    wrefresh(win);
}



void display_game_log(GameLog *log) {
    wclear(log_win);
    box(log_win, 0, 0);
    
    // Log title
    wattron(log_win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    mvwprintw(log_win, 0, (LOG_WIDTH - 10) / 2, " GAME LOG ");
    wattroff(log_win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    
    // Log message
    wattron(log_win, COLOR_PAIR(TEXT_PAIR));
    for (int i = 0; i < log->count && i < MAX_LOG_MESSAGES; i++) {
        mvwprintw(log_win, i + 1, 2, "➤ %s", log->messages[i]);
    }
    wattroff(log_win, COLOR_PAIR(TEXT_PAIR));
    
    wrefresh(log_win);
}


void init_game_log(GameLog *log) {
    log->count = 0;
    for (int i = 0; i < MAX_LOG_MESSAGES; i++) {
        log->messages[i][0] = '\0';
    }
}

void update_game_log(GameLog *log, const char *message) {
    if (log->count >= MAX_LOG_MESSAGES) {
        for (int i = 0; i < MAX_LOG_MESSAGES - 1; i++) {
            strcpy(log->messages[i], log->messages[i + 1]);
        }
        strcpy(log->messages[MAX_LOG_MESSAGES - 1], message);
    } else {
        strcpy(log->messages[log->count], message);
        log->count++;
    }
}



int display_main_menu(void) {
    const char *menu_items[] = {
        "New Game",
        "Game Rules",
        "Exit"
    };
    int selected = 0;
    int ch;

    while (1) {
        clear();
        // Draw title box
        int title_width = 40;
        int title_start_x = MENU_OFFSET_X - title_width/4;
        draw_box(MENU_OFFSET_Y - 4, title_start_x - 2, 3, title_width);
        
        attron(COLOR_PAIR(TITLE_PAIR) | A_BOLD);
        mvprintw(MENU_OFFSET_Y - 3, MENU_OFFSET_X - 5, "BATTLESHIP GAME");
        attroff(COLOR_PAIR(TITLE_PAIR) | A_BOLD);

        draw_box(MENU_OFFSET_Y - 1, MENU_OFFSET_X - 7, MENU_ITEMS_COUNT + 2, 20);

        for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
            if (i == selected) {
                attron(COLOR_PAIR(SELECTED_PAIR) | A_BOLD);
                mvprintw(MENU_OFFSET_Y + i, MENU_OFFSET_X - 5, "> %s", menu_items[i]);
                attroff(COLOR_PAIR(SELECTED_PAIR) | A_BOLD);
            } else {
                attron(COLOR_PAIR(MENU_PAIR));
                mvprintw(MENU_OFFSET_Y + i, MENU_OFFSET_X - 5, "  %s", menu_items[i]);
                attroff(COLOR_PAIR(MENU_PAIR));
            }
        }

        attron(COLOR_PAIR(TEXT_PAIR));
        mvprintw(MENU_OFFSET_Y + MENU_ITEMS_COUNT + 2, MENU_OFFSET_X - 12, 
                "Use UP/DOWN arrows or W/S to navigate");
        mvprintw(MENU_OFFSET_Y + MENU_ITEMS_COUNT + 3, MENU_OFFSET_X - 12, 
                "Press ENTER or SPACE to select");
        attroff(COLOR_PAIR(TEXT_PAIR));

        refresh();

        ch = getch();
        switch (ch) {
            case KEY_UP:
            case 'w':
            case 'W':
                selected = (selected - 1 + MENU_ITEMS_COUNT) % MENU_ITEMS_COUNT;
                break;
            case KEY_DOWN:
            case 's':
            case 'S':
                selected = (selected + 1) % MENU_ITEMS_COUNT;
                break;
            case '\n':
            case ' ':
                return selected;
        }
    }
}

void display_rules(void) {
    clear();
    
    int title_width = 40;
    int title_start_x = MENU_OFFSET_X - title_width/4;
    draw_box(MENU_OFFSET_Y - 4, title_start_x - 2, 3, title_width);
    
    attron(COLOR_PAIR(TITLE_PAIR) | A_BOLD);
    mvprintw(MENU_OFFSET_Y - 3, MENU_OFFSET_X - 5, "GAME RULES");
    attroff(COLOR_PAIR(TITLE_PAIR) | A_BOLD);

    draw_box(MENU_OFFSET_Y - 1, MENU_OFFSET_X - 15, 18, 50);

    attron(COLOR_PAIR(TEXT_PAIR));
    int start_y = MENU_OFFSET_Y;
    mvprintw(start_y++, MENU_OFFSET_X - 12, "1. Each player has a fleet of ships:");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "- 1 Battleship (4 cells)");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "- 2 Cruisers (3 cells each)");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "- 2 Destroyers (2 cells each)");
    
    start_y++;
    mvprintw(start_y++, MENU_OFFSET_X - 12, "2. Ships cannot touch each other");
    mvprintw(start_y++, MENU_OFFSET_X - 12, "3. Ships can be placed horizontally or vertically");
    mvprintw(start_y++, MENU_OFFSET_X - 12, "4. Aim to sink all enemy ships");
    
    start_y++;
    mvprintw(start_y++, MENU_OFFSET_X - 12, "Game Controls:");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "ESC: Open game menu");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "W/S or Up/Down: Navigate menus");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "Enter/Space: Select option");
    
    start_y++;
    mvprintw(start_y++, MENU_OFFSET_X - 12, "Symbols:");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "~ : Water");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "# : Ship");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "X : Hit");
    mvprintw(start_y++, MENU_OFFSET_X - 10, "O : Miss");
    attroff(COLOR_PAIR(TEXT_PAIR));

    attron(COLOR_PAIR(MENU_PAIR));
    mvprintw(start_y + 1, MENU_OFFSET_X - 12, "Press any key to return to main menu...");
    attroff(COLOR_PAIR(MENU_PAIR));
    
    refresh();
    getch();
}

int display_game_menu(void) {
    const char *menu_items[] = {
        "Continue Game",
        "Relocate Ships",
        "Toggle Grid View",
        "Exit the game!"
    };
    int selected = 0;
    int ch;
    
    // Get terminal dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Dimensions for the menu window
    int menu_height = GAME_MENU_ITEMS_COUNT + 4;
    int menu_width = 30;
    int start_y = (max_y - menu_height) / 2;
    int start_x = (max_x - menu_width) / 2;
    
    // Create menu window
    WINDOW *menu_win = newwin(menu_height, menu_width, start_y, start_x);
    keypad(menu_win, TRUE);
    
    while (1) {
        
        box(menu_win, 0, 0);
        
        //Title
        wattron(menu_win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
        mvwprintw(menu_win, 0, (menu_width - 9) / 2, " MENU ");
        wattroff(menu_win, COLOR_PAIR(TITLE_PAIR) | A_BOLD);
        
        // Menu choices
        for (int i = 0; i < GAME_MENU_ITEMS_COUNT; i++) {
            if (i == selected) {
                wattron(menu_win, COLOR_PAIR(SELECTED_PAIR) | A_BOLD);
                mvwprintw(menu_win, i + 2, 2, "> %s", menu_items[i]);
                wattroff(menu_win, COLOR_PAIR(SELECTED_PAIR) | A_BOLD);
            } else {
                wattron(menu_win, COLOR_PAIR(MENU_PAIR));
                mvwprintw(menu_win, i + 2, 2, "  %s", menu_items[i]);
                wattroff(menu_win, COLOR_PAIR(MENU_PAIR));
            }
        }
        
        wrefresh(menu_win);
        
        ch = wgetch(menu_win);
        switch (ch) {
            case KEY_UP:
            case 'w':
            case 'W':
                selected = (selected - 1 + GAME_MENU_ITEMS_COUNT) % GAME_MENU_ITEMS_COUNT;
                break;
            case KEY_DOWN:
            case 's':
            case 'S':
                selected = (selected + 1) % GAME_MENU_ITEMS_COUNT;
                break;
            case '\n':
            case ' ':
                delwin(menu_win);
                return selected;
            case 27:  // ESC tuşu
                delwin(menu_win);
                return GAME_MENU_CONTINUE;
        }
    }
}


int main() {
    // Initialize ncurses
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);  // Enable key input for the main window
    curs_set(0);          // Hide the cursor
    setup_colors();
    init_windows();
    
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    while (1) {
        int choice = display_main_menu();
        
        switch (choice) {
            case MENU_NEW_GAME: {
                clear();  // Clear menu
                refresh(); // Refresh screen
                in_game = 1;
                // Initialize the game variables
                char player1_grid[SIZEofGRID][SIZEofGRID];
                char player2_grid[SIZEofGRID][SIZEofGRID];
                int player1_attacks[SIZEofGRID][SIZEofGRID] = {0};
                int player2_attacks[SIZEofGRID][SIZEofGRID] = {0};
                struct shot_message p1_shot;
                struct shot_message p2_shot;
                memset(&p1_shot, 0, sizeof(struct shot_message));
                memset(&p2_shot, 0, sizeof(struct shot_message));
                
                AIStrategy player1_ai;
                AIStrategy player2_ai;

                // Start game
                srand(time(NULL));
                initialize_strategy(&player1_ai);
                initialize_strategy(&player2_ai);
                playground(player1_grid);
                playground(player2_grid);
                place_all_ships(player1_grid, 'P');
                place_all_ships(player2_grid, 'C');

                clear();
                update_display(player1_grid, player2_grid); 

                // Starting message
                show_status_message("Game Starting...", MOVE_DELAY);

                // Create fifo
                unlink(FIFO_FILE);
                if (mkfifo(FIFO_FILE, 0666) == -1) {
                    endwin();
                    perror("Failed to create FIFO");
                    exit(1);
                }

                pid_t pid = fork();
                if (pid < 0) {
                    endwin();
                    perror("Fork failed");
                    unlink(FIFO_FILE);
                    exit(1);
                }

                // Game loop
                while (!p1_shot.gameOver && !p2_shot.gameOver) {
                    
                    int new_y, new_x;
                    getmaxyx(stdscr, new_y, new_x);
                    if (new_y != max_y || new_x != max_x) {
                        max_y = new_y;
                        max_x = new_x;
                        clear();
                        refresh();
                        delwin(grid1_win);
                        delwin(grid2_win);
                        if (msg_win != NULL) {
                            delwin(msg_win);
                            msg_win = NULL;
                        }
                        init_windows();
                    }

                    

                    // Menu check
                    timeout(100);  
                    int ch = wgetch(stdscr);
                    timeout(-1);  

                    if (ch == 27) { // ESC 
                        clear();
                        refresh();
                        
                        int menu_choice = display_game_menu();
                        switch (menu_choice) {
                            case GAME_MENU_CONTINUE:
                                clear();
                                break;
                            case GAME_MENU_RELOCATE:
                                playground(player1_grid);
                                playground(player2_grid);
                                place_all_ships(player1_grid, 'P');
                                place_all_ships(player2_grid, 'C');
                                show_status_message("Ships have been relocated!", MOVE_DELAY);
                                clear();
                                break;
                            case GAME_MENU_TOGGLE_VIEW:
                                show_grid = !show_grid;
                                show_status_message(show_grid ? "Grid view enabled" : "Grid view disabled", MOVE_DELAY/2);
                                clear();
                                break;
                            case EXIT:
                                if (pid == 0) {
                                    kill(getppid(), SIGTERM);
                                    exit(0);
                                } else {
                                    kill(pid, SIGTERM);
                                    wait(NULL);
                                    unlink(FIFO_FILE);
                                    return MENU_EXIT;  
                                }

                        }
                        update_display(player1_grid, player2_grid);
                        refresh();
                        continue;
                    }

                    if (pid == 0) {  // Player 2 (Child process)
                        char msg[100];
                        snprintf(msg, sizeof(msg), "Player 2 is thinking...");
                        show_status_message(msg, MOVE_DELAY/2);
                        
                        execute_smart_attack(&p2_shot.row, &p2_shot.col, player2_attacks, &player2_ai);
                        player2_attacks[p2_shot.row][p2_shot.col] = 1;

                        snprintf(msg, sizeof(msg), "Player 2 attacks %c%d", 'A' + p2_shot.row, p2_shot.col);
                        show_status_message(msg, MOVE_DELAY);

                        int fd = open(FIFO_FILE, O_WRONLY);
                        write(fd, &p2_shot, sizeof(p2_shot));
                        close(fd);

                        fd = open(FIFO_FILE, O_RDONLY);
                        read(fd, &p2_shot, sizeof(p2_shot));
                        close(fd);
                    
                        update_display(player1_grid, player2_grid);

                        if (p2_shot.hit_or_missed) {
                            player2_ai.hunting_mode = 1;
                            player2_ai.last_hit_row = p2_shot.row;
                            player2_ai.last_hit_col = p2_shot.col;
                            add_target_positions(&player2_ai, p2_shot.row, p2_shot.col, player2_attacks);
                        }

                        if (p2_shot.gameOver) {
                            show_status_message("Player 2 Wins!", END_GAME_DELAY);
                            break;
                        }

                    } else {  // Player 1 (Parent process)
                        int fd = open(FIFO_FILE, O_RDONLY);
                        read(fd, &p2_shot, sizeof(p2_shot));
                        close(fd);
                        update_display(player1_grid, player2_grid);
                        // Process Player 2's move
                        if (check_hit(player1_grid, p2_shot.row, p2_shot.col)) {
                            player1_grid[p2_shot.row][p2_shot.col] = 'X';
                            p2_shot.hit_or_missed = 1;
                            update_display(player1_grid, player2_grid);  
                            char msg[100];
                            snprintf(msg, sizeof(msg), "Player 2 HIT at %c%d!", 'A' + p2_shot.row, p2_shot.col);
                            show_status_message(msg, MOVE_DELAY);
                        } else {
                            player1_grid[p2_shot.row][p2_shot.col] = 'M';
                            p2_shot.hit_or_missed = 0;
                            update_display(player1_grid, player2_grid);  
                            char msg[100];
                            snprintf(msg, sizeof(msg), "Player 2 MISSED at %c%d", 'A' + p2_shot.row, p2_shot.col);
                            show_status_message(msg, MOVE_DELAY/2);
                        }
                        
                        

                        // Check if Player 2 has sunk all the ships
                        int all_ships_hit = 1;
                        for (int i = 0; i < SIZEofGRID && all_ships_hit; i++) {
                            for (int j = 0; j < SIZEofGRID; j++) {
                                if (player1_grid[i][j] == 'P') {
                                    all_ships_hit = 0;
                                    break;
                                }
                            }
                        }

                        if (all_ships_hit) {
                            update_display(player1_grid, player2_grid);
                            show_status_message("Player 2 wins!", END_GAME_DELAY);
                            p2_shot.gameOver = 1;
                            fd = open(FIFO_FILE, O_WRONLY);
                            write(fd, &p2_shot, sizeof(p2_shot));
                            close(fd);
                            break;
                        }

                        // Player 1's turn
                        char msg[100];
                        snprintf(msg, sizeof(msg), "Player 1 is thinking...");
                        show_status_message(msg, MOVE_DELAY/2);
                        
                        execute_smart_attack(&p1_shot.row, &p1_shot.col, player1_attacks, &player1_ai);
                        player1_attacks[p1_shot.row][p1_shot.col] = 1;

                        snprintf(msg, sizeof(msg), "Player 1 attacks %c%d", 'A' + p1_shot.row, p1_shot.col);
                        show_status_message(msg, MOVE_DELAY);

                        if (check_hit(player2_grid, p1_shot.row, p1_shot.col)) {
                            player2_grid[p1_shot.row][p1_shot.col] = 'X';
                            p1_shot.hit_or_missed = 1;
                            update_display(player1_grid, player2_grid);  
                            snprintf(msg, sizeof(msg), "Player 1 HIT at %c%d!", 'A' + p1_shot.row, p1_shot.col);
                            show_status_message(msg, MOVE_DELAY);
                            
                            player1_ai.hunting_mode = 1;
                            player1_ai.last_hit_row = p1_shot.row;
                            player1_ai.last_hit_col = p1_shot.col;
                            add_target_positions(&player1_ai, p1_shot.row, p1_shot.col, player1_attacks);
                        } else {
                            player2_grid[p1_shot.row][p1_shot.col] = 'M';
                            p1_shot.hit_or_missed = 0;
                            update_display(player1_grid, player2_grid); 
                            snprintf(msg, sizeof(msg), "Player 1 MISSED at %c%d", 'A' + p1_shot.row, p1_shot.col);
                            show_status_message(msg, MOVE_DELAY/2);
                        }

                       

                        // Check if Player 1 has sunk all the ships
                        all_ships_hit = 1;
                        for (int i = 0; i < SIZEofGRID && all_ships_hit; i++) {
                            for (int j = 0; j < SIZEofGRID; j++) {
                                if (player2_grid[i][j] == 'C') {
                                    all_ships_hit = 0;
                                    break;
                                }
                            }
                        }

                        if (all_ships_hit) {
                            update_display(player1_grid, player2_grid);  
                            show_status_message("Player 1 wins!", END_GAME_DELAY);
                            p1_shot.gameOver = 1;
                            napms(END_GAME_DELAY);
                            break;
                        }

                        fd = open(FIFO_FILE, O_WRONLY);
                        write(fd, &p2_shot, sizeof(p2_shot));
                        close(fd);
                    }
                }

                // Display the game over status
                clear();
                update_display(player1_grid, player2_grid);
                refresh();
                if (p1_shot.gameOver) {
                    show_status_message("Game Over - Player 1 Wins!", END_GAME_DELAY);
                } else if (p2_shot.gameOver) {
                    show_status_message("Game Over - Player 2 Wins!", END_GAME_DELAY);
                }

                

                //Cleanup
                if (pid == 0) {
                    exit(0);
                } else {
                    kill(pid, SIGTERM);
                    wait(NULL);
                }
                unlink(FIFO_FILE);

return_to_menu:
                // Clean all the windows
                if (msg_win != NULL) {
                    delwin(msg_win);
                    msg_win = NULL;
                }
                if (grid1_win != NULL) {
                    delwin(grid1_win);
                    grid1_win = NULL;
                }
                if (grid2_win != NULL) {
                    delwin(grid2_win);
                    grid2_win = NULL;
                }
                clear();
                refresh();
                init_windows();  
                break;
            }

            case MENU_RULES:
                display_rules();
                break;

            case MENU_EXIT:
                clear(); 
                refresh();
                if (msg_win != NULL) {
                    delwin(msg_win);
                }
                endwin();
                return 0;
        }
    }

    cleanup_windows();
    endwin();
    return 0;
}