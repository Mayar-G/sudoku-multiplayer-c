#ifndef SUDOKU_H
#define SUDOKU_H

#define GRID_SIZE 9
#define MAX_CLIENTS 2

typedef struct {
    int grid[GRID_SIZE][GRID_SIZE];       // grille actuelle du client
    int solution[GRID_SIZE][GRID_SIZE];   // solution correcte
    int fixed[GRID_SIZE][GRID_SIZE];      // cases fixes (données au départ)
} SudokuGame;

// Fonctions
void generate_grid(SudokuGame *game, int level);  // level: 1=facile, 2=moyen, 3=difficile
int  is_valid_move(SudokuGame *game, int row, int col, int val);
int  is_complete(SudokuGame *game);
void grid_to_string(int grid[GRID_SIZE][GRID_SIZE], char *buf);

#endif
