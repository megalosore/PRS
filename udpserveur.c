#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_FILE_BUFFER 150000000
#define BUFFER_SIZE 1500
#define TIMEOUT_VALUE 5

struct sockaddr_in addr_create(int port){ //Create local addr
    struct sockaddr_in my_addr;
    memset((char*)&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    return my_addr;
}

int main(int argc, char* argv[]){
    //Checking args
    if (argc != 2){
        printf("Usage ./serveur <port_udp>\n");
        exit(1);
    }
    //Initialising variables
    char *file_buffer = malloc(MAX_FILE_BUFFER * sizeof(int));
    int port_udp = atoi(argv[1]);
    int currentport = port_udp;
    int reuse = 1;
    int udpsock = socket(AF_INET, SOCK_DGRAM, 0);
    char udpreadbuffer[BUFFER_SIZE];
    char writebuffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;             //Create the struct to store the client connection info
    socklen_t client_taille = (socklen_t)sizeof(client_addr);
    struct sockaddr_in new_client_addr;             //Create the struct to store the client connection info
    socklen_t new_client_taille = (socklen_t)sizeof(new_client_addr);
    struct timeval tv;                          //Preparing a timeout
    tv.tv_sec = TIMEOUT_VALUE;
    tv.tv_usec = 0;
    fd_set fd_select;                           //Create the select struct
    fd_set fs_newselect;

    //Checking socket
    if (udpsock < 0){
        printf("UDP_Socket was not created successfully\n");
        exit(1);
    }
    setsockopt(udpsock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); //Force à libérer le socket lors de la sortie du programme

    //UDP Parameters
    struct sockaddr_in my_udp_addr = addr_create(port_udp);

    bind(udpsock, (struct sockaddr *)&my_udp_addr, sizeof(my_udp_addr));
    printf("Launched UDP server on port %i\n", port_udp);

    while(1){
        FD_ZERO(&fd_select);
        FD_SET(udpsock, &fd_select);
        select(udpsock+1, &fd_select, NULL, NULL, NULL); //Only one socket for now
        if FD_ISSET(udpsock, &fd_select){
            printf("UDP MSG DETECTED\n");
            memset(udpreadbuffer, 0, sizeof(udpreadbuffer));
            recvfrom(udpsock, (char *)udpreadbuffer, sizeof(udpreadbuffer), MSG_WAITALL, ( struct sockaddr *) &client_addr, &client_taille);
            if (strncmp(udpreadbuffer, "SYN", 3)){
                printf("%s\n",udpreadbuffer);
                printf("Non Syn connection ignore\n");
                break;
            }else{
                printf("SYN Received\n");
                currentport++;
                //Create the new Socket
                int newsock;
	            if ( (newsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		            perror("socket creation failed");
		            break;
	            }
                struct sockaddr_in newservaddr = addr_create(currentport);
                bind(newsock, (struct sockaddr *)&newservaddr, sizeof(newservaddr));
                //Sending the ACK and the port
                memset(writebuffer, 0, sizeof(writebuffer));
                snprintf(writebuffer, sizeof(writebuffer), "SYN-ACK %i", currentport);
                sendto(udpsock, writebuffer, sizeof(writebuffer), MSG_CONFIRM, (const struct sockaddr *) &client_addr, client_taille);
                printf("SYN-ACK XXXX Sent\n");
                //reseting the select
                FD_ZERO(&fd_select);
                FD_SET(udpsock, &fd_select);
                //Waiting for ACK

                int timeout = select(port_udp+1, &fd_select, NULL, NULL, &tv);
                if (timeout == -1){
                    printf("Select error line 89\n");
                    exit(1);
                }
                else if (timeout == 0){
                    printf("ACK timeout\n");
                    break;
                }else{
                    memset(udpreadbuffer, 0, sizeof(udpreadbuffer));
                    recvfrom(udpsock, (char *)udpreadbuffer, sizeof(udpreadbuffer), MSG_WAITALL, ( struct sockaddr *) &client_addr, &client_taille);
                    if (strncmp(udpreadbuffer,"ACK",3)){
                        printf("Malformed ACK received\n");
                        exit(1);
                    }

                    printf("3 way handhsake completed\n");
                    memset(udpreadbuffer, 0, sizeof(udpreadbuffer));
                    //Waiting for the filename
                    recvfrom(newsock, (char *)udpreadbuffer, sizeof(udpreadbuffer), MSG_WAITALL, ( struct sockaddr *) &new_client_addr, &new_client_taille);
                    printf("%s\n",udpreadbuffer);

                    //Preparing the file
                    FILE* file = NULL;
                    file = fopen("file.txt", "r");

                    if (file == NULL){
                        printf("File does not exist ERROR\n");
                        exit(1);
                    }

                    //getting file size

                    int file_counter = 0;
                    int to_send;
                    int reread = 0;
                    int remainder;
                    fseek(file, 0, SEEK_END); 
                    int filesize = ftell(file);
                    fseek(file, 0, SEEK_SET); 
                    int to_read = filesize;

                    //reading the file
                    memset(file_buffer, 0, sizeof(writebuffer));
                    memset(writebuffer, 0, sizeof(writebuffer));

                    //sending the file
                    while (1){
                        if (MAX_FILE_BUFFER > to_read){
                            fread(file_buffer, to_read, 1, file);
                            to_send = to_read;
                            to_read = 0;
                            reread = 0;
                        }else{
                            fread(file_buffer, MAX_FILE_BUFFER, 1, file);
                            to_send = MAX_FILE_BUFFER;
                            to_read = to_read - MAX_FILE_BUFFER;
                            reread = 1;
                        }
                        file_counter = 0;
                        remainder = to_send;
                        while(remainder > BUFFER_SIZE){
                            printf("remainder: %i\n",remainder);
                            memset(writebuffer, 0, BUFFER_SIZE);
                            memcpy(writebuffer, file_buffer + (file_counter * BUFFER_SIZE), BUFFER_SIZE);
                            sendto(newsock, writebuffer, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr *) &new_client_addr, new_client_taille);
                            file_counter++;
                            remainder = remainder - BUFFER_SIZE;
                        }
                        if (remainder != 0){
                            memset(writebuffer, 0, BUFFER_SIZE);
                            memcpy(writebuffer, file_buffer + (file_counter * BUFFER_SIZE), remainder);
                            sendto(newsock, writebuffer, remainder, MSG_CONFIRM, (const struct sockaddr *) &new_client_addr, new_client_taille);
                            remainder = 0;
                        }
                        if(reread == 0){
                            break;
                        }
                    }
                    sendto(newsock, "STOP", BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr *) &new_client_addr, new_client_taille);
                    printf("Finished\n");
                    fclose(file);
                    close(newsock);
                    break;
                }
            }
        }
    }
}
