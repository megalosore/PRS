// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
	
#define MAXLINE 1500
	
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
		
	int n;
	socklen_t len;
		
	sendto(sockfd, (const char *)msg_buffer, strlen(msg_buffer),MSG_CONFIRM, (const struct sockaddr *) &servaddr,sizeof(servaddr)); //SYN
	printf("SYN sent.\n");
	n = recvfrom(sockfd, (char *)buffer, MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr,&len); //SYN-ACK
	//Extract the port
	char *strport = strtok(buffer, " ");
	strport = strtok(NULL, " ");
	int newport = atoi(strport);
	buffer[n] = '\0';
	printf("Server : %s and NewPort %i\n", buffer, newport);

	//Sending the ACK
	msg_buffer = "ACK";
	sendto(sockfd, (const char *)msg_buffer, strlen(msg_buffer),MSG_CONFIRM, (const struct sockaddr *) &servaddr,sizeof(servaddr)); //ACK

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
	
	//Asking for the file
	msg_buffer = "file.txt";
	sendto(newsock, (const char *)msg_buffer, strlen(msg_buffer),MSG_CONFIRM, (const struct sockaddr *) &newservaddr,sizeof(newservaddr)); //ACK

	//Receving the file
	FILE* file = NULL;
	int readsize = MAXLINE;
	char seq_buffer[6];
	char ack_buffer[9];
	file = fopen("receivedfile.txt", "w");
	while(1){
		readsize = (int)recvfrom(newsock, (char *)buffer, sizeof(buffer), MSG_WAITALL, ( struct sockaddr *) &newservaddr, &len);
		strncpy(seq_buffer, buffer, 6);
		printf("seq nb: %s | size received: %d\n", seq_buffer, readsize);
		if (strncmp(buffer,"FIN",3)){
			//writting the data
			fwrite(buffer+6, readsize-6 , 1 ,file);
			memset(buffer, 0, sizeof(buffer));
			//sending ACK
			memset(ack_buffer, 0, sizeof(ack_buffer));
			snprintf(ack_buffer, 9, "ACK%s", seq_buffer);
			sendto(newsock, (const char *)ack_buffer, sizeof(ack_buffer), MSG_CONFIRM, (const struct sockaddr *) &newservaddr,sizeof(newservaddr)); //ACK
		}else{
			fclose(file);
			break;
		}
	}
	close(sockfd);
	close(newsock);
	return 0;
}

