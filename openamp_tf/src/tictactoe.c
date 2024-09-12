#include "tictactoe.h"
#include <stdio.h>
#include <limits.h>

#define X     'X'
#define O     'O'
#define EMPTY ' '

/**
 * @brief Initialize the board to the initial state
 */
void initial_state(char board[3][3])
{
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			board[i][j] = EMPTY;
		}
	}
}

/**
 * @brief Determine the current player
 * @param board The current board
 * @return char The current player
 */
char player(char board[3][3])
{
	int x_count = 0, o_count = 0;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (board[i][j] == X) {
				x_count++;
			} else if (board[i][j] == O) {
				o_count++;
			}
		}
	}
	return (x_count > o_count) ? O : X;
}

/**
 * @brief Determine the available actions
 * @param board The current board
 * @param action_list The list of available actions
 * @return int The number of available actions
 */
int actions(char board[3][3], struct action action_list[9])
{
	int count = 0;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (board[i][j] == EMPTY) {
				action_list[count].row = i;
				action_list[count].col = j;
				count++;
			}
		}
	}
	return count;
}

/**
 * @brief Apply a move and return a new board
 * @param new_board The new board
 * @param board The current board
 * @param action The action to apply
 */
void result(char new_board[3][3], char board[3][3], struct action action)
{
	for (int k = 0; k < 3; k++) {
		for (int l = 0; l < 3; l++) {
			new_board[k][l] = board[k][l];
		}
	}
	new_board[action.row][action.col] = player(board);
}

/**
 * @brief Check for a winner
 * @param board The current board
 * @return char The winner
 */
char winner(char board[3][3])
{
	for (int i = 0; i < 3; i++) {
		if (board[i][0] == board[i][1] && board[i][0] == board[i][2] &&
		    board[i][0] != EMPTY) {
			return board[i][0];
		}
		if (board[0][i] == board[1][i] && board[0][i] == board[2][i] &&
		    board[0][i] != EMPTY) {
			return board[0][i];
		}
	}
	if (board[0][0] == board[1][1] && board[0][0] == board[2][2] && board[0][0] != EMPTY) {
		return board[0][0];
	}
	if (board[0][2] == board[1][1] && board[0][2] == board[2][0] && board[0][2] != EMPTY) {
		return board[0][2];
	}
	return EMPTY;
}

/**
 * @brief Check if the game is over
 * @param board The current board
 * @return int 1 if the game is over, 0 otherwise
 */
int terminal(char board[3][3])
{
	if (winner(board) != EMPTY) {
		return 1;
	}
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (board[i][j] == EMPTY) {
				return 0;
			}
		}
	}
	return 1;
}

/**
 * @brief Calculate the utility of the board
 * @param board The current board
 * @return int The utility of the board
 */
int utility(char board[3][3])
{
	char win = winner(board);
	if (win == X) {
		return 1;
	} else if (win == O) {
		return -1;
	} else {
		return 0;
	}
}

// Maximize function
int Max_Value(char board[3][3]);

// Minimize function
int Min_Value(char board[3][3])
{
	if (terminal(board)) {
		return utility(board);
	}

	int v = INT_MAX;
	struct action action_list[9];
	int actions_count = actions(board, action_list);

	for (int i = 0; i < actions_count; i++) {
		char new_board[3][3];
		result(new_board, board, action_list[i]);
		int max_val = Max_Value(new_board);
		if (max_val < v) {
			v = max_val;
		}
	}
	return v;
}

// Maximize function
int Max_Value(char board[3][3])
{
	if (terminal(board)) {
		return utility(board);
	}

	int v = INT_MIN;
	struct action action_list[9];
	int actions_count = actions(board, action_list);

	for (int i = 0; i < actions_count; i++) {
		char new_board[3][3];
		result(new_board, board, action_list[i]);
		int min_val = Min_Value(new_board);
		if (min_val > v) {
			v = min_val;
		}
	}
	return v;
}

/**
 * @brief Minimax algorithm
 * @param board The current board
 * @param best_move The best move
 */
void minimax(char board[3][3], struct action *best_move)
{
	if (terminal(board)) {
		best_move->row = -1;
		best_move->col = -1;
		return;
	}

	int best_val;
	if (player(board) == X) {
		best_val = INT_MIN;
	} else {
		best_val = INT_MAX;
	}

	struct action action_list[9];
	int actions_count = actions(board, action_list);

	for (int i = 0; i < actions_count; i++) {
		char new_board[3][3];
		result(new_board, board, action_list[i]);

		int current_val;
		if (player(board) == X) {
			current_val = Min_Value(new_board);
			if (current_val > best_val) {
				best_val = current_val;
				best_move->row = action_list[i].row;
				best_move->col = action_list[i].col;
			}
		} else {
			current_val = Max_Value(new_board);
			if (current_val < best_val) {
				best_val = current_val;
				best_move->row = action_list[i].row;
				best_move->col = action_list[i].col;
			}
		}
	}
}

/**
 * @brief Draw the board
 * @param board The current board
 */
void draw(char board[3][3])
{
	printf("\033[H\033[J"); // clear screen
	printf("\t0\t1\t2\n\n");
	for (int i = 0; i < 3; i++) {
		printf("%d", i);
		for (int j = 0; j < 3; j++) {
			printf("\t%c", board[i][j]);
		}
		printf("\n\n");
	}
}

char BOARD[3][3];
int user_turn;
char play_marker;

static void setup()
{
	initial_state(BOARD);
	printf("Do you want to play first? (1 for yes, 0 for no): ");
	scanf("%d", &user_turn);
	user_turn = user_turn ? 1 : 0;
	play_marker = user_turn ? X : O;
}

static void loop()
{
	struct action best_move;
	struct action user_move;

	while (!terminal(BOARD)) {
		draw(BOARD);
		printf("You are %c\n", play_marker);

		if (user_turn) {
			printf("Enter your move (row column): ");
			while (1) {
				scanf("%d %d", &user_move.row, &user_move.col);
				if (user_move.row < 0 || user_move.row > 2 || user_move.col < 0 ||
				    user_move.col > 2) {
					printf("Invalid move, try again: ");
					continue;
				}
				if (BOARD[user_move.row][user_move.col] == EMPTY) {
					break;
				}
				printf("Invalid move, try again: ");
			}
			BOARD[user_move.row][user_move.col] = player(BOARD);
			user_turn = 0;
		} else {
			minimax(BOARD, &best_move);
			if (best_move.row == -1) {
				break;
			}
			BOARD[best_move.row][best_move.col] = player(BOARD);
			user_turn = 1;
		}
	}

	draw(BOARD);
	printf("You are %c\n", play_marker);
	if (winner(BOARD) == X) {
		printf("X wins!\n");
	} else if (winner(BOARD) == O) {
		printf("O wins!\n");
	} else {
		printf("It's a draw!\n");
	}
}
