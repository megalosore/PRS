// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
	
#define MAXLINE 1024
	
// Driver code
int main(int argc, char* argv[]) {
	if (argc != 2){
        printf("Usage ./serveur <port_udp>\n");
        return 1;
    }
	int portudp = atoi(argv[1]);
	int sockfd;
	char buffer[MAXLINE];
	char *msg_buffer = "SYN";
	struct sockaddr_in	 servaddr;
	
	// Creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
		
	// Filling server information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(portudp);
	servaddr.sin_addr.s_addr = INADDR_ANY;
		
	int n, len;
		
	sendto(sockfd, (const char *)msg_buffer, strlen(msg_buffer),MSG_CONFIRM, (const struct sockaddr *) &servaddr,sizeof(servaddr)); //SYN
	printf("SYN sent.\n");
	n = recvfrom(sockfd, (char *)buffer, MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr,&len); //SYN-ACK
	//Extract the port
	char *strport = strtok(buffer, " ");
	strport = strtok(NULL, " ");
	int newport = atoi(strport);
	buffer[n] = '\0';
	printf("Server : %s and NewPort %i\n", buffer, newport);

	//Creating new Socket
	int newsock;
	if ( (newsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in	 newservaddr;
	memset(&newservaddr, 0, sizeof(newservaddr));
	newservaddr.sin_family = AF_INET;
	newservaddr.sin_port = htons(newport);
	newservaddr.sin_addr.s_addr = INADDR_ANY;
	//Sending the ACK to the new port
	msg_buffer = "ACK";
	sendto(newsock, (const char *)msg_buffer, strlen(msg_buffer),MSG_CONFIRM, (const struct sockaddr *) &newservaddr,sizeof(newservaddr)); //ACK
	
	//Receving the file
	FILE* file = NULL;
	file = fopen("receivedfile.txt", "w");
	while(1){
		recvfrom(newsock, (char *)buffer, sizeof(buffer), MSG_WAITALL, ( struct sockaddr *) &newservaddr, &len);
		if (strcmp(buffer,"STOP")){
			fwrite(buffer, 1024 ,1 ,file);
			memset(buffer, 0, sizeof(buffer));
		}else{
			break;
		}
	}
	close(sockfd);
	close(newsock);
	return 0;
}

