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
    int file_counter = 0;
    int port_udp = atoi(argv[1]);
    int currentport = port_udp;
    int reuse = 1;
    int udpsock = socket(AF_INET, SOCK_DGRAM, 0);
    char udpreadbuffer[1024];
    char writebuffer[1024];
    struct sockaddr_in client_addr;             //Create the struct to store the client connection info
    int client_taille = sizeof(client_addr);
    struct sockaddr_in new_client_addr;             //Create the struct to store the client connection info
    int new_client_taille = sizeof(new_client_addr);
    struct timeval tv;                          //Preparing a timeout of 10s
    tv.tv_sec = 5;
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
            recvfrom(udpsock, (char *)udpreadbuffer, 1024, MSG_WAITALL, ( struct sockaddr *) &client_addr, &client_taille);
            if (!strcmp(udpreadbuffer, "SYN\n")){
                printf("%s\n",udpreadbuffer);
                printf("Non Syn connection ignore\n");
                break;
            }else{
                printf("%s\n",udpreadbuffer);
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
                snprintf(writebuffer, 1024, "SYN-ACK %i\n", currentport);
                sendto(udpsock, writebuffer, 1024, MSG_CONFIRM, (const struct sockaddr *) &client_addr, client_taille);
                //reseting the select
                FD_ZERO(&fs_newselect);
                FD_SET(newsock, &fs_newselect);
                //Waiting for ACK
                int timeout = select(currentport+1, &fs_newselect, NULL, NULL, &tv);
                if (timeout == -1){
                    printf("Select error line 89\n");
                    exit(1);
                }
                else if (timeout == 0){
                    printf("ACK timeout\n");
                    break;
                }else{
                    printf("3 way handhsake completed\n");
                    recvfrom(newsock, (char *)udpreadbuffer, sizeof(udpreadbuffer), MSG_WAITALL, ( struct sockaddr *) &new_client_addr, &new_client_taille);
                    printf("%s\n",udpreadbuffer);
                    FILE* file = NULL;
                    file = fopen("file.txt", "r");
                    while (!feof(file)){
                        file_counter++;
                        fread(writebuffer,sizeof(writebuffer),1,file);
                        sendto(newsock, writebuffer, sizeof(writebuffer), MSG_CONFIRM, (const struct sockaddr *) &new_client_addr, new_client_taille);
                        memset(writebuffer, 0, sizeof(writebuffer));
                    }
                    sendto(newsock, "STOP", 4, MSG_CONFIRM, (const struct sockaddr *) &new_client_addr, new_client_taille);
                    int file_size = ftell(file);
                    printf("file_size = %i\n");
                    //Communication is open bar
                    break;
                }
            }
        }
    }
}
