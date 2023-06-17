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
    const int BUFFER_SIZE = 1024*1024;
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
        exit(1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) { // Set the SO_REUSEADDR flag
        perror("setsockopt(SO_REUSEADDR) failed!");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    int ret_pton = inet_pton(AF_INET, ip, &serv_addr.sin_adrr);
    if (ret_pton <= 0) {
        perror("inet_pton error!");
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error while connecting to socket!");
        exit(1);
    }

    N = lseek(fd, 0, SEEK_END); // lseek returns the offset in bytes of the third arg which is the end of the file.
    if (N < 0){
        perror("Error in lseek!");
        close(fd);
        exit(1);
    }
    if(lseek(fd, 0, SEEK_SET) < 0){ // Return it to the start of the file, so we can read later
        perror("Error in lseek!");
        close(fd);
        exit(1);
    }

    int bytes_sent = 0, bytes_sent_total = 0;
    uint32_t temp_N = htonl(N);
    while(sizeof(temp_N) - bytes_sent_total > 0){
        bytes_sent = write(sockfd, ((uint8_t*)&temp_N) + bytes_sent_total, sizeof(temp_N) - bytes_sent_total);
        if (bytes_sent <= 0){
            perror("Error sending N value");
            close(fd);
            exit(1);
        }
        bytes_sent_total += bytes_sent;
    }


}