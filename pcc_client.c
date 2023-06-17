#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>


void print_and_exit();

static int recv_sig = 0; // flag for SIGINT, so we know when we received a signal

void mySignalHandler(int signum) { recv_sig = 1; }


int main(int argc, char *argv[]) {

    struct sigaction newAction = {.sa_handler = mySignalHandler,
            .sa_flags = SA_RESTART};
    if (sigaction(SIGINT, &newAction, NULL) == -1) {
        perror("Signal handle registration failed");
        exit(1);
    }

    int listenfd = -1;
    int connfd = -1;

    uint16_t port;
    uint32_t N;

    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);

    uint32_t pcc_total[127] = {0};
    int BUFFER_SIZE = 1024 * 1024; // max 1MB of data buffer size
    char file_data[BUFFER_SIZE];

    if (argc != 2) {
        perror("invalid argument count!");
        exit(1);
    }
    port = atoi(argv[1]);
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket initialization failed!")
        exit(1);
    }
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) { // Set the SO_REUSEADDR flag
        perror("setsockopt(SO_REUSEADDR) failed!");
        exit(1);
    }
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (0 != bind(listenfd,
                  (struct sockaddr *) &serv_addr,
                  addrsize)) {
        perror("bind failed!");
        exit(1);
    }
    if (0 != listen(listenfd, 10)) {
        perror("listen failed!");
        exit(1);
    }

    while (1) {
        if (recv_sig == 1) { // Check for a received SIGINT before we accept a new connection
            print_and_exit(); // If we got a SIGINT and also yet to process a new request, close the server and print the count
        }
        connfd = accept(listenfd, (struct sockaddr *) &peer_addr, &addrsize);
        if (connfd < 0) {
            perror("accept failed!");
            exit(1);
        }
        int reset_flag = 0;
        // First we read the value of N
        int bytes_read = 0, bytes_read_total = 0; // N size is 32bit * in network byte order *
        uint32_t temp;
//        char *n_buff = (char *) &temp; /* Here we will store the bytes we read for N (they are in network byte order)
//                                          (read expects a char pointer) */
        while (bytes_left > 0) { // Use a loop, so we can make sure all bytes were read
            bytes_read = read(connfd, temp + bytes_read_total, sizeof(temp) - bytes_read_total);

            if (bytes_read < 0){
                if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ // If its one of these errors we do not close the server
                    perror("Error occurred while reading N!, Server is still listening for new connections...");
                    close(connfd);
                    reset_flag = 1;
                    break;
                }
                else{ // Different error so we terminate the server
                    perror("Error occurred while reading N!, Server is closing...");
                    close(connfd);
                    exit(1);
                }
            }
            else if(bytes_read == 0){ // If a syscall return 0 it means the client connection terminated
                perror("Error occurred while reading N!, Server is still listening for new connections...");
                close(connfd);
                reset_flag = 1;
                break;
            }

            bytes_read_total += bytes_read;
            bytes_left -= bytes_read_total;
        }

        if (reset_flag == 1){
            continue; // Means we got an error during reading N and closed the connection, so we want to accept a new one
        }


    }
}

void print_and_exit() {
    for (int i = 32; i < 127; ++i) {
        printf("char '%c' : %u times\n", i, pcc_total[i]);
    }
    exit(0);
}