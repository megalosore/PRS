#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_FILE_BUFFER 1466 * 100000 //142.48 Megabytes of memory used for the buffer
#define BUFFER_SIZE 1472
#define TIMEOUT_VALUE 5
#define MAX(x, y) (x > y ? x : y)
#define MIN(x, y) (x < y ? x : y)

struct sockaddr_in addr_create(int port){ //Create local addr
    struct sockaddr_in my_addr;
    memset((char*)&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    return my_addr;
}

void sendSegmentByNumber(int sock, struct sockaddr_in client_addr, socklen_t client_size, int segmentNumber,char *writebuffer,char *file_buffer,char *ackbuffer, int *remainder,int msgSize){
            int file_counter = (segmentNumber-1)%100000;
            memset(writebuffer, 0, BUFFER_SIZE);
            snprintf(writebuffer, 7, "%d", segmentNumber);
            memcpy(writebuffer + 6, file_buffer + (file_counter * (BUFFER_SIZE-6)), msgSize-6); // -6 account for the char used by the seq number
            sendto(sock, writebuffer, msgSize, MSG_CONFIRM, (const struct sockaddr *) &client_addr, client_size);
            *remainder = *remainder - (msgSize - 6);
}

int checkAck(int sock,time_t rtt, int lastAck, int seq_nb){
    //Return the new value that segmentNumber should take;
    struct timeval tv = {0, 2000 + rtt};//20 time the round trip measured just to be safe
    int timeout_flag;
    char ackbuffer[10];
    int duplicateAck[2];
    duplicateAck[0] = lastAck; //The number of the ack we are waiting for
    duplicateAck[1] = 0;                           //Number of duplicate ack
    fd_set select_ack;
    while(duplicateAck[0] < (seq_nb-1)){
        memset(ackbuffer, 0, 10);
        FD_ZERO(&select_ack);
        FD_SET(sock, &select_ack);
        timeout_flag = select(sock+1, &select_ack, NULL, NULL, &tv); //wait for an ack

        if(!timeout_flag){//retransmit after time out 
            //printf("Timeout: No ACK Received for seq %d\n",segmentNumber);
            return duplicateAck[0];
        }
        else{ //ACK received
            recv(sock, ackbuffer, sizeof(ackbuffer)+1, MSG_WAITALL);
            //Extract the ackNum
            char strACKNum[7];
            memcpy(strACKNum, ackbuffer+3,7);
            int ackNum = atoi(strACKNum);
            //printf("received ACK%i\n", ackNum);

            if (duplicateAck[0] == ackNum){ //If we receive an already received ack do ++
                duplicateAck[1] += 1;
            }else if ((duplicateAck[0] < ackNum) || (ackNum == 0)){ //If we receive a Higher ACK update the num Special case wit the ACK0
                    duplicateAck[0] = ackNum;
                    duplicateAck[1] = 1;
            }
        }
        if (duplicateAck[1] >= 3){ //retransmit after 3 duplicate
            //printf("Duplicate: Three duplicate ACK Received for seq %d\n",duplicateAck[0]);
            recv(sock, NULL, 1500 * 100, MSG_DONTWAIT); //Emptying the buffer queue
            return duplicateAck[0];
        }
    }
    return duplicateAck[0]; // If no duplicate send back the last ack received 
}

void send_file(FILE* fd, int sock, struct sockaddr_in client_addr, socklen_t client_size, time_t rtt){ //read and send the file to the remote host
    //Initializing usefull variables
    int to_send;
    int seq_nb = 0;
    int reread = 1;
    int garbage = 0;
    int remainder;
    
    int maxwindowSize=100;
    int windowSize=20; //Every value are possible 
    int lastAck = 0;
    int tmpAck = 0;
    
    //Various buffers
    char *file_buffer = malloc(MAX_FILE_BUFFER * sizeof(char)); //We will load the file in memory before sending (it is faster)
    char *writebuffer = malloc(BUFFER_SIZE * sizeof(char));
    char *ackbuffer = malloc(10 * sizeof(char));

    //getting file size
    fseek(fd, 0, SEEK_END); 
    int to_read = ftell(fd);
    rewind(fd);

    
    while(reread){
        seq_nb++;
        //Loading the file in the memory
        if (MAX_FILE_BUFFER > to_read){
            //The whole file fit in the buffer
            fread(file_buffer, to_read, 1, fd);
            to_send = to_read;
            to_read = 0;
            reread = 0;
        }else{
            //We need to reread the file after sending the previous part
            fread(file_buffer, MAX_FILE_BUFFER, 1, fd);
            to_send = MAX_FILE_BUFFER;
            to_read = to_read - MAX_FILE_BUFFER;
        }
        //file_counter = 0;
        remainder = to_send;
        int nbOfPacketAlreadySent=0;
        int lastRemainder=0;
        while(remainder != 0 || nbOfPacketAlreadySent!=0){
            while(remainder > windowSize*(BUFFER_SIZE - 6)){
                for (;nbOfPacketAlreadySent<windowSize; nbOfPacketAlreadySent++){
                    sendSegmentByNumber(sock,client_addr,client_size, seq_nb,writebuffer,file_buffer,ackbuffer,&remainder,BUFFER_SIZE);
                    seq_nb++;
                }
                tmpAck = lastAck;
                lastAck = checkAck(sock, rtt, lastAck, seq_nb);
                nbOfPacketAlreadySent -= (lastAck - tmpAck);
                if (lastAck == seq_nb-1){ //No loss
                    windowSize = MIN(maxwindowSize, windowSize+1);
                }else{
                    if (lastAck != tmpAck){
                        windowSize = MAX(1,windowSize/2);
                    }
                    sendSegmentByNumber(sock,client_addr,client_size, lastAck+1,writebuffer,file_buffer,ackbuffer,&garbage,BUFFER_SIZE);
                }
                 //printf("Waiting %d ACK,window: %d, lastAck: %d, seqn: %d\n", nbOfPacketAlreadySent, windowSize, lastAck, seq_nb);
            }
            if ((remainder > 0) && (remainder <= windowSize*(BUFFER_SIZE - 6))){
                //printf("Remainder: %d\n", remainder);
                for (;nbOfPacketAlreadySent<windowSize; nbOfPacketAlreadySent++){
                    if (remainder > BUFFER_SIZE - 6){
                        sendSegmentByNumber(sock,client_addr,client_size, seq_nb,writebuffer,file_buffer,ackbuffer,&remainder,BUFFER_SIZE);
                        seq_nb++;
                    }else if (remainder > 0){
                        lastRemainder = remainder;
                        sendSegmentByNumber(sock,client_addr,client_size,seq_nb,writebuffer,file_buffer,ackbuffer,&remainder,remainder+6);
                        seq_nb++;
                        //printf("last2 Seq nb : %d, lastACK : %d, nbOfPacketAlreadySent %d, remainder: %d\n",seq_nb, lastAck,nbOfPacketAlreadySent, remainder);
                        break;
                    }
                }
                tmpAck = lastAck;
                lastAck = checkAck(sock, rtt, lastAck, seq_nb);
                nbOfPacketAlreadySent -= (lastAck - tmpAck);                
                
                if ((remainder == 0)){
                    while (lastAck < seq_nb - 1){
                        //printf("Waiting for ACK: %d, lastAck: %d, seqn: %d\n", nbOfPacketAlreadySent, lastAck, seq_nb);
                        if (lastAck == seq_nb - 2){
                            sendSegmentByNumber(sock,client_addr,client_size, lastAck+1,writebuffer,file_buffer,ackbuffer,&garbage,lastRemainder+6);
                        }else{
                            sendSegmentByNumber(sock,client_addr,client_size, lastAck+1,writebuffer,file_buffer,ackbuffer,&garbage,BUFFER_SIZE);
                        }
                        lastAck = checkAck(sock, rtt, lastAck, seq_nb);
                    }
                    nbOfPacketAlreadySent = 0;
                }
                else if (lastAck==seq_nb-2){
                    sendSegmentByNumber(sock,client_addr,client_size, lastAck+1,writebuffer,file_buffer,ackbuffer,&garbage,lastRemainder+6);   
                }else{
                    sendSegmentByNumber(sock,client_addr,client_size, lastAck+1,writebuffer,file_buffer,ackbuffer,&garbage,BUFFER_SIZE);//if lastAck<seqnb
                }
            }
        }
    }

    for (int i=0;i<100;i++){ //FUCCKKKKKKKK STOP YOU DAMNIT
        //printf("FIN\n");
        sendto(sock, "FIN", 3, MSG_CONFIRM, (const struct sockaddr *) &client_addr, client_size);
    }
    printf("Finished\n");
    free(writebuffer);
    free(file_buffer);
    free(ackbuffer);
    sleep(1);
    sendto(sock, "FIN", 3, MSG_CONFIRM, (const struct sockaddr *) &client_addr, client_size);
    //I WILL MAKE YOU STOP IF YOU DIDNT (Do not remove the client is DUMB)
}

int main(int argc, char* argv[]){
    //Checking args
    if (argc != 2){
        printf("Usage ./serveur <port_udp>\n");
        exit(1);
    }
    //Initialising variables
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
    fd_set fd_select;

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
                snprintf(writebuffer, sizeof(writebuffer), "SYN-ACK%i", currentport);
                
                struct timeval tv = {TIMEOUT_VALUE, 0};                 //Prepare a timeout
                struct timeval start, end;                              //Mesure RTT
                gettimeofday(&start, NULL);
                sendto(udpsock, writebuffer, sizeof(writebuffer), MSG_CONFIRM, (const struct sockaddr *) &client_addr, client_taille);
                printf("SYN-ACK XXXX Sent\n");
                //reseting the select
                FD_ZERO(&fd_select);
                FD_SET(udpsock, &fd_select);

                //Waiting for ACK
                int timeout = select(port_udp+1, &fd_select, NULL, NULL, &tv);
                if (timeout == -1){
                    printf("Select error line 89\n");
                    break;
                }
                else if (timeout == 0){
                    printf("ACK timeout\n");
                    break;
                }else{
                    gettimeofday(&end, NULL);
                    time_t rtt = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    memset(udpreadbuffer, 0, sizeof(udpreadbuffer));
                    recvfrom(udpsock, (char *)udpreadbuffer, sizeof(udpreadbuffer), MSG_WAITALL, ( struct sockaddr *) &client_addr, &client_taille);
                    if (strncmp(udpreadbuffer,"ACK",3)){
                        printf("Malformed ACK received\n");
                        exit(1);
                    }

                    printf("3 way handhsake completed| RTT is %ld us\n", rtt);
                    memset(udpreadbuffer, 0, sizeof(udpreadbuffer));
                    //Waiting for the filename
                    recvfrom(newsock, (char *)udpreadbuffer, sizeof(udpreadbuffer), MSG_WAITALL, ( struct sockaddr *) &new_client_addr, &new_client_taille);
                    printf("%s\n",udpreadbuffer);

                    //Preparing the file
                    FILE* file = NULL;
                    file = fopen(udpreadbuffer, "r");
                    printf("here1\n");
                    if (file == NULL){
                        printf("File does not exist ERROR\n");
                        break;
                    }

                    send_file(file, newsock, new_client_addr, new_client_taille, rtt);

                    fclose(file);
                    close(newsock);
                }
            }
        }
    }
}
