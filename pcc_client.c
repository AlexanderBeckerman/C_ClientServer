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

int main(int argc, char *argv[]) {

    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    char *file_path, ip, port;
    int sockfd = -1;
    uint32_t N, count;
    const int BUFFER_SIZE = 1024 * 1024;
    char file_data[BUFFER_SIZE];


    if (argc != 4) {
        perror("invalid argument count!");
        exit(1);
    }

    ip = argv[1];
    port = argv[2];
    file_path = argv[3];

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file!");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket initialization failed!");
        close(sockfd);
        exit(1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) { // Set the SO_REUSEADDR flag
        perror("setsockopt(SO_REUSEADDR) failed!");
        close(sockfd);
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    int ret_pton = inet_pton(AF_INET, ip, &serv_addr.sin_adrr); // Translate ip from string
    if (ret_pton <= 0) {
        perror("inet_pton error!");
        close(sockfd);
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error while connecting to socket!");
        close(sockfd);
        exit(1);
    }

    N = lseek(fd, 0, SEEK_END); // lseek returns the offset in bytes of the third arg which is the end of the file.
    if (N < 0) {
        perror("Error in lseek!");
        close(fd);
        close(sockfd);
        exit(1);
    }
    if (lseek(fd, 0, SEEK_SET) < 0) { // Return it to the start of the file, so we can read later
        perror("Error in lseek!");
        close(fd);
        close(sockfd);
        exit(1);
    }

    int bytes_sent = 0, bytes_sent_total = 0;
    uint32_t temp_N = htonl(N);
    while (sizeof(temp_N) - bytes_sent_total > 0) { // Send N to the server
        bytes_sent = write(sockfd, ((uint8_t * ) & temp_N) + bytes_sent_total, sizeof(temp_N) - bytes_sent_total);
        if (bytes_sent <= 0) {
            perror("Error sending N value");
            close(fd);
            close(sockfd);
            exit(1);
        }
        bytes_sent_total += bytes_sent;
    }
    // Send the file data to the server
    bytes_sent = 0;
    int data_read = 0, total_data_read = 0, data_to_read = 0, bytes_to_send, bytes_sent_buffer = 0;
    bytes_sent_total = 0;

    while (N - bytes_sent_total > 0) {
        if (N - total_data_read <
            BUFFER_SIZE) { // Get the size we want to read into the buffer = MAX(BUFFER_SIZE, remaining data)
            data_to_read = N - total_data_read;
        } else {
            data_to_read = BUFFER_SIZE;
        }
        bytes_to_send = data_to_read;
        total_data_read = 0;
        while (data_to_read > 0) { // We loop over data_to_read to make sure we fill our buffer
            data_read = read(fd, file_data + total_data_read, data_to_read);
            if (data_read <= 0) {
                perror("Error reading from file!");
                close(fd);
                close(sockfd);
                exit(1);
            }
            data_to_read -= data_read;
            total_data_read += data_read;
        }
        bytes_sent_buffer = 0;
        bytes_sent = 0;
        while (bytes_to_send > 0) { // Loop to make sure all the data inside the buffer was sent to the server
            bytes_sent = write(sockfd, file_data + bytes_sent_buffer, bytes_to_send);
            if (bytes_sent <= 0) {
                perror("Error sending file data!");
                close(fd);
                close(sockfd);
                exit(1);
            }
            bytes_to_send -= bytes_sent;
            bytes_sent_buffer += bytes_sent;
        }

        bytes_sent_total += bytes_sent_buffer;

    }
    // Read the count from the server
    int bytes_read = 0, bytes_read_total = 0;
    uint32_t temp; // count size is 32bit * in network byte order * so we use a temporary variable to store it in and later convert it
    while (sizeof(temp) - bytes_read_total > 0) { // Use a loop, so we can make sure all bytes were read

        // Convert the pointer to uint8, so we can read with granularity of 1 byte and make sure all bytes were read.
        bytes_read = read(sockfd, ((uint8_t * ) & temp) + bytes_read_total, sizeof(temp) - bytes_read_total);
        if (bytes_read <= 0) {
            perror("Error reading count from server!");
            close(fd);
            close(sockfd);
            exit(1);
        }

        bytes_read_total += bytes_read;
    }
    count = ntohl(temp);
    printf("# of printable characters: %u\n", count);
    exit(0);


}