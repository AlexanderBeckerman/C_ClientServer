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


void print_and_exit(void);


static uint32_t pcc_total[127] = {0}; // Our main pcc data struct

void mySignalHandler(int signum) {

    print_and_exit(); // When we get here it means we unblocked SIGINT and are not connected to a client
}


int main(int argc, char *argv[]) {

    struct sigaction newAction = {.sa_handler = mySignalHandler,
            .sa_flags = SA_RESTART};
    if (sigaction(SIGINT, &newAction, NULL) == -1) { // Create and initialize our SIGINT handler
        perror("Signal handle registration failed");
        exit(1);
    }

    int listenfd = -1;
    int connfd = -1;

    uint16_t port;
    uint32_t N;
    uint32_t count = 0;

    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);


    uint32_t pcc_curr[127] = {
            0}; // Create a temporary data struct for current connection, so we can  update the pcc data structure only if the connection is completed without errors

    const int BUFFER_SIZE = 1024 * 1024; // max 1MB of data buffer size
    char file_data[BUFFER_SIZE];

    if (argc != 2) {
        perror("invalid argument count!");
        exit(1);
    }
    port = atoi(argv[1]);
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket initialization failed!");
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

    sigset_t mask; // I will use this to block the SIGINT until we finish the connection
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);

    while (1) {

        sigprocmask(SIG_UNBLOCK, &mask, NULL); // Unblock it here in case we restarted the loop without closing the server
        connfd = accept(listenfd, (struct sockaddr *) &peer_addr, &addrsize);
        sigprocmask(SIG_BLOCK, &mask, NULL); // If we got out from accept it means we are connected so we block SIGINT until we disconnect

        if (connfd < 0) {
            perror("accept failed!");
            exit(1);
        }

        int reset_flag = 0; // Flag for knowing when we want to continue to accept a new connection
        // First we read the value of N
        int bytes_read = 0, bytes_read_total = 0;
        uint32_t temp; // N size is 32bit * in network byte order * so we use a temporary variable to store it in and later convert it
        while (sizeof(temp) - bytes_read_total > 0) { // Use a loop, so we can make sure all bytes were read

            // Convert the pointer to uint8, so we can read with granularity of 1 byte and make sure all bytes were read.
            bytes_read = read(connfd, ((uint8_t * ) & temp) + bytes_read_total, sizeof(temp) - bytes_read_total);
            if (bytes_read <= 0) {
                if (errno == ETIMEDOUT || errno == ECONNRESET ||
                    errno == EPIPE || bytes_read == 0) { // If its one of these errors we do not close the server
                    perror("Error occurred while reading N!, Server is still listening for new connections...");
                    if (close(connfd) < 0) {
                        perror("Error closing connection!");
                        exit(1);
                    }
                    reset_flag = 1; // Turn on the flag, so we can continue the outer loop and accept a new connection
                    break; // Break out of current read loop
                } else { // Different error so we terminate the server
                    perror("Error occurred while reading N!, Server is closing...");
                    exit(1);
                }
            }
            bytes_read_total += bytes_read;
        }

        if (reset_flag == 1) {
            continue; // Means we got an error during reading N and closed the connection, so we want to accept a new one
        }
        // Now we want to read the file data from the socket
        N = ntohl(temp); // Convert the N we got from network byte order
        bytes_read = 0;
        bytes_read_total = 0;
        int bytes_to_read = 0; // Use this, so we know how many bytes to fit into the buffer
        while (bytes_read_total < N) {
            if (N - bytes_read_total >
                BUFFER_SIZE) { //  Get the size we want to read into the buffer = MIN(BUFFER_SIZE, remaining data)
                bytes_to_read = BUFFER_SIZE;
            } else {
                bytes_to_read = N - bytes_read_total;
            }
            bytes_read = read(connfd, file_data, bytes_to_read); // Read them into our buffer file_data
            if (bytes_read <= 0) {
                if (errno == ETIMEDOUT || errno == ECONNRESET ||
                    errno == EPIPE || bytes_read == 0) { // If its one of these errors we do not close the server
                    perror("Error occurred while reading file data!, Server is still listening for new connections...");
                    if (close(connfd) < 0) {
                        perror("Error closing connection!");
                        exit(1);
                    }
                    reset_flag = 1;
                    break;
                } else { // Different error so we terminate the server
                    perror("Error occurred while reading file data!, Server is closing...");
                    exit(1);
                }
            }
            bytes_read_total += bytes_read;


            for (int i = 0;
                 i < bytes_read; ++i) { // Get how many printable chars and update the temporary data structure
                if (file_data[i] >= 32 && file_data[i] <= 126) {
                    count++;
                    pcc_curr[(int) file_data[i]]++;
                }
            }
        }
        if (reset_flag == 1) {
            continue;
        }
        // Lastly, send the count back to the client
        int bytes_sent = 0;
        int bytes_sent_total = 0;
        uint32_t new_count = htonl(count);
        while (sizeof(new_count) - bytes_sent_total > 0) {
            // Convert the pointer to uint8, so we can write with granularity of 1 byte and make sure all bytes were sent.
            bytes_sent = write(connfd, ((uint8_t * ) & new_count) + bytes_sent_total,
                               sizeof(new_count) - bytes_sent_total);

            if (bytes_sent <= 0) {
                if (errno == ETIMEDOUT || errno == ECONNRESET ||
                    errno == EPIPE || bytes_sent == 0) { // If its one of these errors we do not close the server
                    perror("Error occurred while sending count!, Server is still listening for new connections...");
                    if (close(connfd) < 0) {
                        perror("Error closing connection!");
                        exit(1);
                    }
                    reset_flag = 1;
                    break;
                } else { // Different error so we terminate the server
                    perror("Error occurred while sending count!, Server is closing...");
                    exit(1);
                }
            }

            bytes_sent_total += bytes_sent;
        }
        if (reset_flag == 1) {
            continue;
        }

        // Copy the added counts to the main data structure after the connection completed without errors
        for (int i = 32; i < 127; ++i) {
            pcc_total[i] += pcc_curr[i]; // Add the count
            pcc_curr[i] = 0; // Reset the temp structure for next connection
        }

        if(close(connfd) < 0){
            perror("Error closing connection!");
            exit(1);
        }
        sigprocmask(SIG_UNBLOCK, &mask, NULL); // We finished the connection so we unblock SIGINT
        count = 0; // Reset the count of printable chars
    }
}

void print_and_exit() {
    for (int i = 32; i < 127; ++i) {
        printf("char '%c' : %u times\n", i, pcc_total[i]);
    }
    exit(0);
}