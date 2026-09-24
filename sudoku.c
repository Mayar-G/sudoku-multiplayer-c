#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sudoku.h"

// Grilles prédéfinies (facile/moyen/difficile)
static int base_solution[GRID_SIZE][GRID_SIZE] = {
    {5,3,4,6,7,8,9,1,2},
    {6,7,2,1,9,5,3,4,8},
    {1,9,8,3,4,2,5,6,7},
    {8,5,9,7,6,1,4,2,3},
    {4,2,6,8,5,3,7,9,1},
    {7,1,3,9,2,4,8,5,6},
    {9,6,1,5,3,7,2,8,4},
    {2,8,7,4,1,9,6,3,5},
    {3,4,5,2,8,6,1,7,9}
};

// Nombre de cases masquées selon le niveau
void generate_grid(SudokuGame *game, int level) {
    int holes = (level == 1) ? 30 : (level == 2) ? 45 : 55;

    // Copier la solution
    memcpy(game->solution, base_solution, sizeof(base_solution));
    memcpy(game->grid, base_solution, sizeof(base_solution));
    memset(game->fixed, 1, sizeof(game->fixed));

    // Masquer des cases aléatoirement
    int removed = 0;
    while (removed < holes) {
        int r = rand() % GRID_SIZE;
        int c = rand() % GRID_SIZE;
        if (game->grid[r][c] != 0) {
            game->grid[r][c] = 0;
            game->fixed[r][c] = 0;
            removed++;
        }
    }
}

int is_valid_move(SudokuGame *game, int row, int col, int val) {
    // Vérifier que la case n'est pas fixe
    if (game->fixed[row][col]) return 0;
    // Vérifier que la valeur correspond à la solution
    if (game->solution[row][col] != val) return 0;
    return 1;
}

int is_complete(SudokuGame *game) {
    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            if (game->grid[i][j] == 0) return 0;
    return 1;
}

void grid_to_string(int grid[GRID_SIZE][GRID_SIZE], char *buf) {
    int pos = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            pos += sprintf(buf + pos, "%d ", grid[i][j]);
        }
        pos += sprintf(buf + pos, "\n");
    } } 
