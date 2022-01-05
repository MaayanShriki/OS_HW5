#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>

#define READ 'r'
#define WRITE 'w'
#define W_ERR_MSG "Couldn't write message"
#define R_ERR_MSG "Couldn't read message"

int sock_fd = -1;

unsigned int numOfBytes(char *filename) {
    struct stat sb;

    if (stat(filename, &sb) == -1) {
        perror("Couldn't determine the number of bytes if file.");
        exit((EXIT_FAILURE));
    }
    return (unsigned int) sb.st_size;
}

char *FileToBuffer(char *filename, unsigned int N) {
    FILE *f;
    char *buffer = (char *) malloc(N * sizeof(char));

    if ((f = fopen(filename, "rb")) == NULL) {
        perror("Couldn't open the specified file");
        exit(EXIT_FAILURE);
    }

    if (fread(buffer, 1, N, f) != N) {
        perror("Couldn't read all data");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

void wr(char wr, char *buffer, uint32_t max_bytes, char *err_msg){
    ssize_t total_wr = 0, bytes_wr = 0;
    while (total_wr < max_bytes) {
        if (wr == WRITE) bytes_wr = write(sock_fd, buffer + total_wr, max_bytes - total_wr);
        if (wr == READ) bytes_wr = read(sock_fd, buffer + total_wr, max_bytes - total_wr);
        if (bytes_wr < 0){
            perror(err_msg);
            exit(EXIT_FAILURE);
        }
        total_wr += bytes_wr;
    }
}

int main(int argc, char *argv[]) {
    uint32_t N, buf_N;
    char *buffer;
    struct sockaddr_in server;

    if (argc != 4) {
        perror("exactly 4 inputs are required.");
        exit(EXIT_FAILURE);
    }
    N = numOfBytes(argv[3]);
    buffer = FileToBuffer(argv[3], N);

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Couldn't create socket");
        exit(EXIT_FAILURE);
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server.sin_addr.s_addr);
    if (connect(sock_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("couldn't connect");
        exit(EXIT_FAILURE);
    }

    buf_N = htonl(N);

    wr(WRITE, (char *) &buf_N, 4, W_ERR_MSG);
    wr(WRITE, buffer, N, W_ERR_MSG);
    wr(READ, (char *) &buf_N, 4, R_ERR_MSG);

    close(sock_fd);
    printf("# of printable characters: %u\n", ntohl(buf_N));
    return 0;
}