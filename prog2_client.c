#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include "prog2.h"

void receiveState() {

}

void sendGuess() {

}

void setupOutput(const GameInfo* params) {
	char p=params->playerNum;
	int oppNumber=(atoi(&p)%2)+1;
	printf("You are Player %c... ",p);
	printf("the game will begin when Player %d joins.\n",oppNumber);
	printf("Board size: %d\n",params->boardSize);
	printf("Seconds per turn: %d\n",params->roundDuration);
}

// null byte not needed since board is generated as char array with no spaces
void displayBoard(const char* board, int boardSize) {
	printf("Board: ");
	for (int i=0;i<boardSize;i++) {
		printf("%c ", board[i]);
	}
	printf("\n");
}

void roundOutput(const GameState* cur, const char playerNum) {
	printf("Round %d:\n", cur->roundNumber);
	printf("Score: ");
	if (playerNum=='2') printf("%d - %d\n", cur->score2, cur->score1);
	else printf("%d - %d\n", cur->score1, cur->score2);
}

void endOutput(const GameState* cur, const char playerNum) {
	bool p1_win = (cur->score1==3) && (playerNum=='1');
	bool p2_win = (cur->score2==3) && (playerNum=='2');
	if (p1_win || p2_win) printf("You won!\n");
	else printf("You Lost.\n");
}

int main(int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	GameInfo* gp; /* game parameters: p1 or p2, board size, and time/round */
	GameState* cur; /* current game state */
	char input[256]; /* buffer to hold the user's input */
	char *host; /* pointer to host name */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	int  n; /* number of characters read */
	
	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if(argc != 3) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	host = argv[1];
	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
		sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}
	if (recv(sd, &gp, sizeof(gp), 0) != sizeof(gp)){
		fprintf(stderr,"Error: Receiving gp failed\n");			
		exit(EXIT_FAILURE);
	}
	setupOutput(&gp);
	/*
	while (1) {
		int n;
		cur <- get current game state
		if (cur->score1 < 3 && cur->score2 < 3) {
			roundOutput(cur,gp->playerNum);
			displayBoard(cur->board, gp->boardSize);
			get turn status
			turnOutput(turnStatus);
			do turn;
		}
		else {
			endOutput(cur,gp->playerNum);
			break; 
		}
	}
	*/
	close(sd);
	exit(EXIT_SUCCESS);
}
/*
	func prompt_and_send() {
		prompt
		send
	}

	func recv_and_validate() {
		recv
		validate
	}

	func turn() {
		recv game state -> myTurnOrder
		
		func_array[myTurnOrder]
		func_array[(myTurnOrder + 1) % 2]
	}

	func round() {
		recv round number
		recv score
		recv board
		while (!invalid) {
		
			turn()

		}
	}
	\

	funct invalid() {
		word is guessesd this round or word is not in global server dict or word has letter not in board
		for letter in word:
			if l
	}
*/