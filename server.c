#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */

// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", (pkt->ack || pkt->dupack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "", pkt->dupack ? " DUP-ACK": "");
}

void printTimeout(struct packet* pkt) {
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) RTO/1000000;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

//not working shifting function
// void shift_num(int arr[], int num2move){
//     int num2Cap=WND_SIZE-num2move;
//     for (int i=0;i!=num2move;i++){
//         for (int j=0;j!=WND_SIZE;j++){
//             arr[j]=arr[j+1];      
//             }
//     }
//     for (int k= num2Cap-1;k!=WND_SIZE;k++){
//         arr[k]=0;
//     }

// }

// =====================================

int main (int argc, char *argv[])
{
    if (argc != 2) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    unsigned int servPort = atoi(argv[1]);

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        perror("bind() error");
        exit(1);
    }

    int cliaddrlen = sizeof(cliaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================

    unsigned short seqNum = (rand() * rand()) % MAX_SEQN;

    for (int i = 1; ; i++) {
        // =====================================
        // Establish Connection: This procedure is provided to you directly and
        // is already working.

        int n;

        FILE* fp;

        struct packet synpkt, synackpkt, ackpkt;

        while (1) {
            n = recvfrom(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
            if (n > 0) {
                printRecv(&synpkt);
                if (synpkt.syn)
                    break;
            }
        }

        unsigned short cliSeqNum = (synpkt.seqnum + 1) % MAX_SEQN; // next message from client should have this sequence number

        buildPkt(&synackpkt, seqNum, cliSeqNum, 1, 0, 1, 0, 0, NULL);

        while (1) {
            printSend(&synackpkt, 0);
            sendto(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
            
            while(1) {
                n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
                if (n > 0) {
                    printRecv(&ackpkt);
                    if (ackpkt.seqnum == cliSeqNum && ackpkt.ack && ackpkt.acknum == (synackpkt.seqnum + 1) % MAX_SEQN) {

                        int length = snprintf(NULL, 0, "%d", i) + 6;
                        char* filename = malloc(length);
                        snprintf(filename, length, "%d.file", i);

                        fp = fopen(filename, "w");
                        free(filename);
                        if (fp == NULL) {
                            perror("ERROR: File could not be created\n");
                            exit(1);
                        }

                        fwrite(ackpkt.payload, 1, ackpkt.length, fp);

                        seqNum = ackpkt.acknum;
                        cliSeqNum = (ackpkt.seqnum + 1) % MAX_SEQN;

                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                        break;
                    }
                    else if (ackpkt.syn) {
                        buildPkt(&synackpkt, seqNum, (synpkt.seqnum + 1) % MAX_SEQN, 1, 0, 0, 1, 0, NULL);
                        break;
                    }
                }
            }

            if (! ackpkt.syn)
                break;
        }

        // *** TODO: Implement the rest of reliable transfer in the server ***
        // Implement GBN for basic requirement or Selective Repeat to receive bonus

        // Note: the following code is not the complete logic. It only expects 
        //       a single data packet, and then tears down the connection
        //       without handling data loss.
        //       Only for demo purpose. DO NOT USE IT in your final submission
        struct packet recvpkt;
        unsigned short startSeqNum=(cliSeqNum-1)%MAX_SEQN;
        int receivedBM[WND_SIZE];
        int pktLength[WND_SIZE];
        for (int i=0;i!=WND_SIZE;i++){
            receivedBM[i]=0;
            pktLength[i]=0;
        }
        char bufferTenPkts[PAYLOAD_SIZE*WND_SIZE];
        int cur_start=1;
        int full=1;

        while(1) {
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
            if (n > 0) {
                printRecv(&recvpkt);
                

                if (recvpkt.fin) {
                    cliSeqNum = (recvpkt.seqnum + 1) % MAX_SEQN;

                    buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                    printSend(&ackpkt, 0);
                    sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                    break;
                }
                else
                {
                    
                    int incoming_pkt=(recvpkt.seqnum-startSeqNum)%MAX_SEQN;

                    if(incoming_pkt>=cur_start+WND_SIZE || incoming_pkt < cur_start-WND_SIZE){
                        continue;
                    }

                    cliSeqNum = (recvpkt.seqnum + 1) % MAX_SEQN;

                    if (receivedBM[incoming_pkt-cur_start]==1 || cur_start>incoming_pkt){
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                    }
                    else
                    {
                        int cur_index=incoming_pkt-cur_start;
                        receivedBM[cur_index]=1;
                        pktLength[cur_index]=recvpkt.length;

                        if (incoming_pkt==cur_start){
                            memcpy(bufferTenPkts+ (incoming_pkt - cur_start) * PAYLOAD_SIZE, recvpkt.payload, recvpkt.length);
                            buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                            printSend(&ackpkt, 0);
                            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                            
                            full = cur_start;
                            int i=0;
                            int count=0;
                            for (;i!=WND_SIZE && receivedBM[i]==1;i++){ 
                                count++; 
                                full++;
                            } // count is now the number of received pkts

                            int bytes2write=pktLength[i-1]+ (count-1) * PAYLOAD_SIZE;
                            fwrite(bufferTenPkts, 1, bytes2write, fp);
                            memcpy(bufferTenPkts, bufferTenPkts+bytes2write, WND_SIZE*PAYLOAD_SIZE-bytes2write);    //move buffer forward
                            
                            int num2move=(full-cur_start);
                            int numuntilCap=cur_start + WND_SIZE - full;
                            // printf("num2move: %d | full: %d | curstart: %d\n",num2move, full, cur_start);
                            // printf("1: ");
                            // for (int g=0;g!=WND_SIZE;g++){
                            //     printf("bit: %d",receivedBM[g]);
                            // }
                            // printf("\n");
                            memcpy((int *) receivedBM, (int *) (receivedBM + num2move), sizeof(int) * numuntilCap);
                            // printf("2: ");
                            // for (int g=0;g!=WND_SIZE;g++){
                            //     printf("bit: %d",receivedBM[g]);
                            // }
                            // printf("\n");
                            // shift_num(receivedBM, num2move);
                            memset((int *) (receivedBM + numuntilCap), 0, (full - cur_start) * sizeof(int));
                            // printf("3: ");
                            // for (int g=0;g!=WND_SIZE;g++){
                            //     printf("bit: %d",receivedBM[g]);
                            // }
                            // printf("\n");
                            memcpy((int *) pktLength, (int *) (pktLength + num2move), sizeof(int) * numuntilCap);

                            
                            cur_start = full;
                        }
                        else{
                            memcpy(bufferTenPkts+ (incoming_pkt - cur_start) * PAYLOAD_SIZE, recvpkt.payload, recvpkt.length);
                            
                            buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                            printSend(&ackpkt, 0);
                            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                        }
                        
                    }   
                }
            }
        }

        // *** End of your server implementation ***

        fclose(fp);
        // =====================================
        // Connection Teardown: This procedure is provided to you directly and
        // is already working.

        struct packet finpkt, lastackpkt;
        buildPkt(&finpkt, seqNum, 0, 0, 1, 0, 0, 0, NULL);
        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);

        printSend(&finpkt, 0);
        sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
        double timer = setTimer();

        while (1) {
            while (1) {
                n = recvfrom(sockfd, &lastackpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
                if (n > 0)
                    break;

                if (isTimeout(timer)) {
                    printTimeout(&finpkt);
                    printSend(&finpkt, 1);
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                    timer = setTimer();
                }
            }

            printRecv(&lastackpkt);
            if (lastackpkt.fin) {

                printSend(&ackpkt, 0);
                sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                printSend(&finpkt, 1);
                sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                timer = setTimer();
                
                continue;
            }
            if ((lastackpkt.ack || lastackpkt.dupack) && lastackpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN)
                break;
        }

        seqNum = lastackpkt.acknum;
    }
}
