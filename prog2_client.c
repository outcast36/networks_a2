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


// null byte not needed since board is generated as char array with no spaces
void displayBoard(const char* board, int boardSize) {
	printf("Board: ");
	for (int i=0;i<boardSize;i++) {
		printf("%c ", board[i]);
	}
	printf("\n");
}


bool receiveState(int sd, int boardSize, const char playerNum) {

	uint8_t score1, score2;
	int roundNumber;
	char board[boardSize];

	if (recv(sd, &score1, sizeof(uint8_t), 0) != sizeof(uint8_t)){
		fprintf(stderr,"Error: Receiving score1 failed\n");			
		exit(EXIT_FAILURE);
	}

	if (recv(sd, &score2, sizeof(uint8_t), 0) != sizeof(uint8_t)){
		fprintf(stderr,"Error: Receiving score2 failed\n");			
		exit(EXIT_FAILURE);
	}
	if (recv(sd, &roundNumber, sizeof(int), 0) != sizeof(int)){
		fprintf(stderr,"Error: Receiving roundNumber failed\n");			
		exit(EXIT_FAILURE);
	}
	if (recv(sd, &board, boardSize, 0) != boardSize){
		fprintf(stderr,"Error: Receiving board failed\n");			
		exit(EXIT_FAILURE);
	}

	if (score1 == 3 || score2 == 3) {
		bool p1_win = (score1==3) && (playerNum=='1');
		bool p2_win = (score2==3) && (playerNum=='2');

		if (p1_win || p2_win) printf("You won!\n");
		else printf("You Lost.\n");

		return false;

	} else {
		printf("\n");
		printf("Round %d...\n", roundNumber);
		printf("Score is ");
		if (playerNum=='2') printf("%d - %d\n", score2, score1);
		else printf("%d-%d\n", score1, score2);
		displayBoard(board, boardSize);
		return true;
	}
}

void setupOutput(const GameInfo* params) {
	char p = params->playerNum;
	printf("You are Player %c... ",p);
	int oppNumber=(atoi(&p)%2)+1;
	
	printf("the game will begin when Player %d joins.\n",oppNumber);
	printf("Board size: %d\n",params->boardSize);
	printf("Seconds per turn: %d\n",params->roundDuration);
}

bool clientTurn(int sd, int roundDuration) {
	char code;
	if (recv(sd, &code, sizeof(code), 0) != sizeof(char)){
		fprintf(stderr,"Error: Receiving code failed\n");			
		exit(EXIT_FAILURE);
	}

	if (code == 'Y') {
		printf("Your turn, enter word: ");
		fflush(stdout);

		char buffer[MAX_WORD_LEN];

	    int fd = fileno(stdin);

	    struct pollfd fds[1];
	    fds[0].fd = STDIN_FILENO;
	    fds[0].events = POLLIN;

	    int poll_result = poll(fds, 1, roundDuration*1000);;
	    if (poll_result > 0) {
	        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		        fprintf(stderr, "Error: fgets failed\n");
		        exit(EXIT_FAILURE);
		    }
	    } else if (poll_result < 0) {
	        fprintf(stderr, "Error: poll failed\n");
	        exit(EXIT_FAILURE);
	    }

	   	uint8_t word_len = poll_result == 0 ? 0 : strlen(buffer) - 1;

	    if (send(sd, &word_len, sizeof(word_len), 0) < 0) {
	    	fprintf(stderr,"Error: Sending word_len failed\n");
	    	exit(EXIT_FAILURE);
	    }

		if (word_len > 0) {
			if (send(sd, &buffer, word_len, 0) < 0) {
				fprintf(stderr,"Error: Sending word failed\n");
		    	exit(EXIT_FAILURE);
			}
		}

		uint8_t ret_code;

		if (recv(sd, &ret_code, sizeof(ret_code), 0) != sizeof(ret_code)){
			fprintf(stderr,"Error: Receiving ret_code failed\n");			
			exit(EXIT_FAILURE);
		}

		if (ret_code == 0) {
			printf("Invalid word!\n");
			return false;
		} else {
			printf("Valid word!\n");
		}

	} else {
		printf("Please wait for opponent to enter word...\n");
		uint8_t word_len;
		
		if (recv(sd, &word_len, sizeof(word_len), 0) != sizeof(word_len)){
			fprintf(stderr,"Error: Receiving word_len failed\n");			
			exit(EXIT_FAILURE);
		}

		if (word_len > 0) {
			char word[word_len+1];
			if (recv(sd, &word, word_len, 0) != word_len){
				fprintf(stderr,"Error: Receiving word failed\n");			
				exit(EXIT_FAILURE);
			}
			word[word_len] = '\0';
			printf("Opponent entered \"%s\"\n", word);
		} else {
			printf("Opponent lost the round!\n");
			return false;
		}
	}
	return true;
}

int main(int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	GameInfo gi; /* game parameters: p1 or p2, board size, and time/round */
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

	if (recv(sd, &gi, sizeof(gi), 0) != sizeof(gi)){
		fprintf(stderr,"Error: Receiving gp failed %d\n", n);			
		exit(EXIT_FAILURE);
	}
	setupOutput(&gi);
	
	while (true) {
		if (!receiveState(sd, gi.boardSize, gi.playerNum)) {
			break;
		}
		while (clientTurn(sd, gi.roundDuration)) {}
	}
	close(sd);
	exit(EXIT_SUCCESS);
}