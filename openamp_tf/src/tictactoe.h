#ifndef TICTACTOE_H
#define TICTACTOE_H

struct action {
	int row;
	int col;
};

/**
 * @brief Minimax algorithm
 * @param board The current board
 * @param best_move The best move
 */
void minimax(char board[3][3], struct action *best_move);

/**
 * @brief Determine the current player
 * @param board The current board
 * @return char The current player
 */
char player(char board[3][3]);

void draw(char board[3][3]);

#endif // !TICTACTOE_H
