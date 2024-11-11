#include "trie.h"

// Game info remains constant over a single instance of a game
typedef struct {
	char playerNum;
	uint8_t boardSize;
	uint8_t roundDuration;
} GameInfo;

// Game state is actively modified as the game progresses
typedef struct {
	TrieNode* guessed;
	char* board;
	int roundNumber;
	int activePlayer;
	uint8_t score1;
	uint8_t score2;
} GameState;

#define MAX_WORD_LEN 100