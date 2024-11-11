#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <poll.h>
#include "trie.h"
#include "prog2.h"

#define QLEN 6 /* size of request queue */

uint8_t boardSize, roundDuration;
TrieNode* wordList;

void generateBoard(char* board) {
	bool containsVowel = false;
	char vowels[5] = {'a', 'e', 'i', 'o', 'u'};
	for (int i = 0; i < boardSize; i++) {
		if (i == boardSize - 1 && !containsVowel) {
			board[i] = vowels[rand() % 5];
		} 
		else {
			char gen = 'a' + (rand() % 26);
			for (int j = 0; j < 5; j++) {
				containsVowel = (containsVowel || (gen == vowels[j]));
			} 
			board[i] = gen;
		}
	}
}

TrieNode* readLexicon(const char* filePath) {
	TrieNode* root = emptyTrie();
	FILE* f = fopen(filePath, "r");
	if (f==NULL) {
		fprintf(stderr,"Error: Opening file failed\n");
		exit(EXIT_FAILURE);
	}
	char line[256];
	while (fgets(line, sizeof(line), f) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		insertWord(root, line);
	}
	fclose(f);
	return root;
}

GameState* initGame() {
	GameState* game = (GameState*) malloc(sizeof(GameState));
	game->board = malloc(boardSize);
	game->roundNumber = 0;
	game->guessed=NULL;
	game->score1 = 0;
	game->score2 = 0;
	return game;
}

// Wrap poll, send, and recv into this function to monitor client health
int transmit(const void* buf, size_t len, int mode, int p1_sock, int p2_sock, int roundLen) {
	struct pollfd clients[2];
    clients[0].fd=p1_sock;
	clients[0].events=POLLIN|POLLOUT; // one or the other 
   	clients[1].fd=p2_sock;
	clients[1].events=POLLIN|POLLOUT; // one or the other
    int client_health = poll(clients,2,roundLen*1000);
	switch (client_health) {
        case 0: // timeout 
        	// poll blocked for timeout seconds and neither client socket became ready:
        	// neither client was able to read or write without blocking so disconnect
            fprintf(stderr,"Error: no client data for N seconds\n");
			break;
    	case -1: // error (disconnect?)
        	fprintf(stderr,"Error: polling clients failed\n");	
            break;
        default:
            // examine for recv possibility -- one client or the other, never both
        	if (clients[0].revents & POLLIN) recv(p1_sock,buf,len,0);
        	if (clients[1].revents & POLLIN) recv(p2_sock,buf,len,0);
        	// examine for send possibility
			// start of turn: Y/N -> check who active player is
			// both cases after guessing word: (1,word), (0,0) -> check who active player is
        	if (clients[0].revents & POLLOUT) {
				bool p1_val=((player1 is inactive) && buf=='N') || ((player1 is active) && buf=='Y');
				if (p1_val) send(p1_sock,buf,sizeof(char),0);
				
				send(p1_sock,buf,len,0);
			}
        	if (clients[1].revents & POLLOUT) send(p2_sock,buf,len,0);
		}
	return client_health;
}
// When a client connects, the server immediately sends struct containing:
// char indicating whether it is player 1 (‘1’) or player 2 (‘2’)
// uint8_t indicating the number of letters on the “board”
// uint8_t indicating the number of seconds you have per turn
int setupClient(struct sockaddr_in *cad, GameInfo* params, int *alen, int sd_server) {
	alen=sizeof(cad);
	int sd_client;
	if ((sd_client = accept4(sd_server,(struct sockaddr *) &cad,&alen,SOCK_NONBLOCK)) < 0) {
		fprintf(stderr, "Error: Accept new player failed\n");
		return -1; 
	}
	if (send(sd_client, params, sizeof(params),0) < 0) {
		fprintf(stderr, "Error: Send client parameters failed\n");
		close(*sd_client);
		return -1; 
	}
	return sd_client;
}

void setupRound(GameState* game) {
	game->roundNumber++;
	// active player is 1 if player 1 starts, 0 if player 2 starts
	game->activePlayer=game->roundNumber%2; 
	generateBoard(game->board);
	clearTrie(game->guessed);
	game->guessed = emptyTrie();
}

int updateClients(GameState* game, int sd_client1, int sd_client2, int roundLen) {
	int c;
	c=transmit(game->score1,sizeof(uint8_t),sd_client1,sd_client2,roundLen);
	if (c<0) return c;
	c=transmit(game->score2,sizeof(uint8_t),sd_client1,sd_client2,roundLen);
	if (c<0) return c;
	c=transmit(game->roundNumber,sizeof(uint8_t),sd_client1,sd_client2,roundLen);
	if (c<0) return c;
	c=transmit(game->board,sizeof(game->board),sd_client1,sd_client2,roundLen);
	if (c<0) return c;
}

int playTurn() {
	int c;
	bool valid=false;
	c=transmit() send 'Y' to active player
	if (c<0) 
	c=transmit() send 'N' to non active player
	if (c<0)
	c=transmit() get word from acitve -> // handled by poll() wait roundDuration seconds for active player to send a word
	if (c>0) valid=validateWord(word);
	
	if (valid) {
		send 1 (valid flag) to active player
		increase active score by 1
		c=1; 
		send word message to inactive player
	}
	else { // handles timeout case since valid is only updated to be true if word is received before poll() times out
		send 0 (invalid flag) to both players
		c=0;
		game->roundNumber++:
		// increase inactive score by 1
		if (game->activePlayer == 0) game->score1++;
		else game->score2++;
		
	}
	return c;
}

// TODO FIX THIS -- letter validation doesnt handle marking each individual letter as used within a single guess:
// e.g. board is b e r -> bee or beer are invalid cause only one e on the board
bool validateWord(GameState* cur, TrieNode* lexicon, const char* word, const int boardSize) {
	TrieNode* roundTrie = cur->guessed;
	int wordLen=strlen(word);
	bool validLetters=true; 
	for (int i=0;i<wordLen;i++) {
		int j=0;
		for (;j<boardSize;j++) {
			if (cur->board[j]==word[i]) break;
		}
		validLetters=(j==boardSize);
	}
	return (!contains(roundTrie,word) && contains(lexicon,word) && validLetters);
}

void playRound() {
	while (c):
		playTurn(activePlayer)
		if (c<0) break;
		switch active and inactive players
	return c;
}

void destroyGame(GameState* game) {
	clearTrie(game->guessed);
	free(game->board);
	free(game);
}

void playGame(int p1_sock, int p2_sock, int roundLen) {	
	GameState* game = initGame();
	int c;
	while (game->score1<3 && game->score2<3) {
		setupRound(game);
		c=updateClients(game,p1_sock,p2_sock,roundLen);
		if (c<0) break;
		c=playRound();
		if (c<0) break;
	}
	destroyGame(game);
	return c;
}

int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	struct sockaddr_in cad1, cad2; /* structure to hold client's address */
	int sd, p1_sock, p2_sock; /* socket descriptors */
	int port; /* protocol port number */
	int p1_alen, p2_alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	int st; /* status value returned by some helper functions */

	if(argc != 5) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server server_port board_size round_length lexicon\n");
		exit(EXIT_FAILURE);
	}

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */
	sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
	port = atoi(argv[1]); /* convert argument to binary */
	if (port > 0) { /* test for illegal value */
		sad.sin_port = htons((u_short)port);
	} 
	else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}
		
	boardSize = atoi(argv[2]);
	roundDuration = atoi(argv[3]);
	wordList = readLexicon(argv[4]);
	// These are sent to clients immediately upon connection
	GameInfo p1 = {'1', boardSize, roundDuration}; 
	GameInfo p2 = {'2', boardSize, roundDuration};
	
	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

	// Loop to handle multiple clients
	while(1){ // TODO ADD TIMEOUT FOR NO CLIENTS TO EXIT AND CLEAR SERVER RESOURCES
		st=setupClient(&cad1,&p1,&p1_alen,&p1_sock,sd);
		if (st<0) continue;
		st=setupClient(&cad2,&p2,&p2_alen,&p2_sock,sd);
		if (st<0) continue;
		pid_t pid = fork();
		if (pid < 0) {
			fprintf(stderr, "Error: Fork to new game process failed\n");
			exit(EXIT_FAILURE);
		}
		if (pid == 0){
			close(sd); // close child access to server's listening port
			playGame(p1_sock,p2_sock);
			// close child access to game port
			close(p1_sock); 
			close(p2_sock);
			exit(EXIT_SUCCESS);
		}
	}
	// Release server resources if no clients connect after a certain point
	close(sd);
	clearTrie(wordList);
	exit(EXIT_SUCCESS);
}