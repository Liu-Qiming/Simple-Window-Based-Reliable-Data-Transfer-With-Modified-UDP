#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h> 

#include <stdbool.h>
// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */
#define FIN_WAIT 2 /* seconds to wait after receiving FIN*/

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

double setFinTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) FIN_WAIT;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

void shift_arr_timer(double arr[]){
    for (int i=0;i!=10;i++){
        arr[i]=arr[i+1];
    }
    arr[9]=-1;
}

void shift_arr_pkt(struct packet arr[]){
    for (int i=0;i!=10;i++){
        arr[i]=arr[i+1];
    }
}


// =====================================

int main (int argc, char *argv[])
{
    if (argc != 4) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    struct in_addr servIP;
    if (inet_aton(argv[1], &servIP) == 0) {
        struct hostent* host_entry; 
        host_entry = gethostbyname(argv[1]); 
        if (host_entry == NULL) {
            perror("ERROR: IP address not in standard dot notation\n");
            exit(1);
        }
        servIP = *((struct in_addr*) host_entry->h_addr_list[0]);
    }

    unsigned int servPort = atoi(argv[2]);

    FILE* fp = fopen(argv[3], "r");
    if (fp == NULL) {
        perror("ERROR: File not found\n");
        exit(1);
    }

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = servIP;
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    int servaddrlen = sizeof(servaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================
    // Establish Connection: This procedure is provided to you directly and is
    // already working.
    // Note: The third step (ACK) in three way handshake is sent along with the
    // first piece of along file data thus is further below

    struct packet synpkt, synackpkt;

    unsigned short seqNum = rand() % MAX_SEQN;
    buildPkt(&synpkt, seqNum, 0, 1, 0, 0, 0, 0, NULL);

    printSend(&synpkt, 0);
    sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    double timer = setTimer();
    int n;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            else if (isTimeout(timer)) {
                printTimeout(&synpkt);
                printSend(&synpkt, 1);
                sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
        }

        printRecv(&synackpkt);
        if ((synackpkt.ack || synackpkt.dupack) && synackpkt.syn && synackpkt.acknum == (seqNum + 1) % MAX_SEQN) {
            seqNum = synackpkt.acknum;
            break;
        }
    }

    // =====================================
    // FILE READING VARIABLES
    
    char buf[PAYLOAD_SIZE];
    size_t m;

    // =====================================
    // CIRCULAR BUFFER VARIABLES

    struct packet ackpkt;
    struct packet pkts[WND_SIZE];
    int s = 0;
    int e = 0;
    int full = 0;

    bool finish = false;

    // =====================================
    // Send First Packet (ACK containing payload)

    m = fread(buf, 1, PAYLOAD_SIZE, fp);
   
    if (m < PAYLOAD_SIZE)
    {
        finish = true;
    }

    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, m, buf);
    printSend(&pkts[0], 0);
    sendto(sockfd, &pkts[0], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 1, m, buf);

    e = 1;

    // =====================================
    // *** TODO: Implement the rest of reliable transfer in the client ***
    // Implement GBN for basic requirement or Selective Repeat to receive bonus

    // Note: the following code is not the complete logic. It only sends a
    //       single data packet, and then tears down the connection without
    //       handling data loss.
    //       Only for demo purpose. DO NOT USE IT in your final submission
    unsigned short first_seq = pkts[0].seqnum; 
    unsigned short expected = first_seq;
    int not_yet_received = 1;
    double timers[WND_SIZE];
    timers[0]=timer;
    for (int k=1;k!=WND_SIZE;k++){
        timers[k]=-1;
    }

    while (1) {
        // n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
        // if (n > 0) {
        //     break;
        // }

        // for now, assume no loss and just send the data
        
        for (int i = 0; i<10; i++)
        {
            //printf("round %d\n", i);
            if (i == 0)
            {
                continue;
            }

            if (finish == true)
            {
                continue;
            }

            if (i != 0)
            {            
                char temp[PAYLOAD_SIZE];
                m = fread(temp, 1, PAYLOAD_SIZE, fp);

                if (m ==0)
                {
                    finish = true;
                    break;
                }

                if (i == 1)
                {
                    expected = (expected + m ) % MAX_SEQN; 
                }

                first_seq = (first_seq + m) % MAX_SEQN;

                buildPkt(&pkts[i], first_seq, 0, 0, 0, 0, 0, m, temp);
                printSend(&pkts[i], 0);
                sendto(sockfd, &pkts[i], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timers[i]=setTimer();

                not_yet_received ++;

                if (m < PAYLOAD_SIZE)
                {
                    finish = true;
                    break;
                }
            }

        }
        if (finish == true && not_yet_received == 0)
        {break;}
        
        //========================================= first round timers/pkts full ===========================================================
        
        while (1)
        {
            //struct packet tempt;
            while(1)
            {
                if (isTimeout(timers[0])){
                    // TODO: retransmission
                    // retrx all pkts[], clear all timers, add timer
                    for (int j = 0; j< 10; j++)
                    {
                        if (timers[j] != -1)
                        {
                            struct packet curr_pkt;
                            curr_pkt = pkts[j];

                            // send all non-empty pkts:
                            printSend(&curr_pkt, 0);
                            sendto(sockfd, &curr_pkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                            timers[j] = setTimer();

                        }
                    }
                }
                n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
                
                if (n>0)
                {
                    break;
                }
            }

            // print receive
            printRecv(&ackpkt); 

            // check if is expected, if yes send new pkt
            not_yet_received --;
            if (not_yet_received == 0)
            {
                break;
            }
            if ((ackpkt.ack || ackpkt.dupack) && ackpkt.acknum == expected)
            {
                
                unsigned short new_seq = (ackpkt.acknum + 9* 512)%MAX_SEQN;

                //struct packet new;

                char holder[PAYLOAD_SIZE];
                m = fread(holder, 1, PAYLOAD_SIZE, fp);

                if (m ==0)
                {
                    finish = true;
                    break;
                }

                shift_arr_timer(timers);
                shift_arr_pkt(pkts);
                buildPkt(&pkts[9], new_seq, 0, 0, 0, 0, 0, m, holder);
                timers[9]=setTimer();
                printSend(&pkts[9], 0);
                sendto(sockfd, &pkts[9], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                // window shifting: pop front, push new pkt if has pkts left, push new timer
                
                expected = expected + 512;
                not_yet_received ++;

                if (m < 512)
                {
                    finish = true;
                    break;
                }
            }
        }
        
        if (finish == true && not_yet_received == 0)
        {
            break;
        }
    }
    // *** End of your client implementation ***
    fclose(fp);

    // =====================================
    // Connection Teardown: This procedure is provided to you directly and is
    // already working.

    struct packet finpkt, recvpkt;
    buildPkt(&finpkt, ackpkt.acknum, 0, 0, 1, 0, 0, 0, NULL);
    buildPkt(&ackpkt, (ackpkt.acknum + 1) % MAX_SEQN, (ackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, 0, NULL);

    printSend(&finpkt, 0);
    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    int timerOn = 1;

    double finTimer;
    int finTimerOn = 0;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            if (timerOn && isTimeout(timer)) {
                printTimeout(&finpkt);
                printSend(&finpkt, 1);
                if (finTimerOn)
                    timerOn = 0;
                else
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
            if (finTimerOn && isTimeout(finTimer)) {
                close(sockfd);
                if (! timerOn)
                    exit(0);
            }
        }
        printRecv(&recvpkt);
        if ((recvpkt.ack || recvpkt.dupack) && recvpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN) {
            timerOn = 0;
        }
        else if (recvpkt.fin && (recvpkt.seqnum + 1) % MAX_SEQN == ackpkt.acknum) {
            printSend(&ackpkt, 0);
            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
            finTimer = setFinTimer();
            finTimerOn = 1;
            buildPkt(&ackpkt, ackpkt.seqnum, ackpkt.acknum, 0, 0, 0, 1, 0, NULL);
        }
    }
}

