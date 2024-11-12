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
#include "prog2.h"

#define QLEN 6 /* size of request queue */

uint8_t boardSize, roundDuration;
TrieNode* wordList;

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

// Poll client socket before sending
int safeSend(void* bufp, size_t len, int sd_client) {
	struct pollfd clients[1];
    clients[0].fd=sd_client;
	clients[0].events=POLLOUT; 

    int client_health = poll(clients,1,5000); // 5 second timeout on server send to client
	switch (client_health) {
        case 0: // timeout 
            fprintf(stderr,"Error: no client data for N seconds\n");
			break;
    	case -1: // error (disconnect?)
        	fprintf(stderr,"Error: polling clients failed\n");	
            break;
        default: // send safely
        	if (clients[0].revents & POLLOUT) send(sd_client,bufp,len,0);
	}
	return client_health;
}

// When a client connects, the server immediately sends struct containing:
// char indicating whether it is player 1 (‘1’) or player 2 (‘2’)
// uint8_t indicating the number of letters on the “board”
// uint8_t indicating the number of seconds you have per turn
int setupClient(GameInfo* params, int sd_server) {
	struct sockaddr_in cad; /* structure to hold client's address */
	socklen_t alen=sizeof(cad);
	int sd_client;
	if ((sd_client = accept(sd_server,(struct sockaddr *) &cad, &alen)) < 0) {
		fprintf(stderr, "Error: Accept new player failed\n");
		return -1; 
	}
	if (safeSend(params, sizeof(GameInfo),sd_client) < 0) {
		fprintf(stderr, "Error: Send client parameters failed\n");
		close(sd_client);
		return -1;
	}
	return sd_client;
}


void setupRound(GameState* game) {
	game->roundNumber++;
	// active player is 1 if player 1 starts, 0 if player 2 starts
	game->activePlayer=(game->roundNumber+1)%2; 
	generateBoard(game->board);
	clearTrie(game->guessed);
	game->guessed = emptyTrie();
}

int updateClients(GameState* game, int sd_client1, int sd_client2) {
	int c;
	for (int i = 0; i < 2; i++) {
		int active_sd = i == 0 ? sd_client1 : sd_client2;
		c=safeSend(&game->score1,sizeof(uint8_t),active_sd);
		if (c<0) return c;
		c=safeSend(&game->score2,sizeof(uint8_t),active_sd);
		if (c<0) return c;
		c=safeSend(&game->roundNumber,sizeof(int),active_sd);
		if (c<0) return c;
		c=safeSend(game->board,sizeof(game->board),active_sd);
		if (c<0) return c;
	}
	return 0;
}

bool validateWord(GameState* game, const char* word) {
	int count[26];
	for (int i=0;i<26;i++) count[i]=0;
	for (int i=0;i<strlen(game->board);i++) {
		count[game->board[i]-'a']++;
	}

	for (int i=0;i<strlen(word);i++) {
		count[word[i]-'a']--;
		if (count[word[i]-'a'] < 0) return false;
	}

    return !contains(game->guessed,word) && contains(wordList, word);
}

void updateScores(GameState* game) {
	// increase inactive score by 1
	if (game->activePlayer == 0) game->score2++;
	else game->score1++;
}

int playRound(GameState* game, int sd_client1, int sd_client2) {
	int c;
	while (true) {
		int active_sd = game->activePlayer == 0 ? sd_client1 : sd_client2;
		int inactive_sd = game->activePlayer == 0 ? sd_client2 : sd_client1;

		// Send active player Y and inactive player N
		char code1 = game->activePlayer == 0 ? 'Y' : 'N';
		char code2 = game->activePlayer == 1 ? 'Y' : 'N';
		c=safeSend(&code1,sizeof(code1),sd_client1);
		if (c<0) break;
		c=safeSend(&code2,sizeof(code2),sd_client2);
		if (c<0) break;

		// Receive word from active player within N seconds
		uint8_t word_len;
		struct pollfd clients[1];
    	clients[0].fd=active_sd;
		clients[0].events=POLLIN; 

		c=poll(clients,1,roundDuration*1000);
		if (c<0) break; 
		else if (c>0) {
			if (recv(active_sd,&word_len,sizeof(uint8_t),0) <= 0) {
				c = -1;
				break;
			}
			char word[word_len+1];
			if (recv(active_sd,&word,word_len,0) <= 0) {
				c = -1;
				break;
			}
			word[word_len] = '\0';

			// Send (1,word) or (0,0) on validation check
			bool valid = validateWord(game, word);
			uint8_t active_ret_code = valid ? 1 : 0;
			uint8_t inactive_ret_code = valid ? word_len : 0;
			c=safeSend(&active_ret_code,sizeof(active_ret_code),active_sd);
			if (c<0) break;
			c=safeSend(&inactive_ret_code,sizeof(inactive_ret_code),inactive_sd);
			if (c<0) break;
			if (valid) {
				insertWord(game->guessed, word);
				c=safeSend(&word,word_len,inactive_sd);
				if (c<0) break;
				game->activePlayer = !game->activePlayer;
			}
			else { //invalid
				updateScores(game);
				break;
			}
		}
		else updateScores(game); //timeout case
	}
	return c;
}


void destroyGame(GameState* game) {
	clearTrie(game->guessed);
	free(game->board);
	free(game);
}

int playGame(int p1_sock, int p2_sock) {	
	GameState* game = initGame();
	int c;
	while (true) {
		setupRound(game);
		c=updateClients(game,p1_sock,p2_sock);
		if (c<0) break;

		if (game->score1==3 || game->score2==3) break;

		c=playRound(game,p1_sock,p2_sock);
		if (c<0) break;
	}
	destroyGame(game);
	return c;
}

int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	int sd, p1_sock, p2_sock; /* socket descriptors */
	int port; /* protocol port number */
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
		st=setupClient(&p1, sd);
		if (st<0) continue;
		else p1_sock = st;

		st=setupClient(&p2, sd);
		if (st<0) continue;
		else p2_sock = st;

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
		break;
	}
	// Release server resources if no clients connect after a certain point
	close(sd);
	clearTrie(wordList);
	exit(EXIT_SUCCESS);
}