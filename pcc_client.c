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

int main(int argc, char *argv[]) {
    int sock_fd = -1;
    uint32_t N, buf_N;
    char *buffer, buf_len[4];
    ssize_t total_wr, bytes_wr = 0;

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

    buf_N = htons(N);
    buf_len[0] = (char) (buf_N & 0xff);
    buf_len[1] = (char) ((buf_N>>8)  & 0xff);
    buf_len[2] = (char) ((buf_N>>16) & 0xff);
    buf_len[3] = (char) ((buf_N>>24) & 0xff);

    total_wr = 0;
    while (total_wr < 4) {
        bytes_wr = write(sock_fd, &buf_len[total_wr], 4 - total_wr);
        if (bytes_wr < 0){
            perror("Couldn't write message");
            exit(EXIT_FAILURE);
        }
        total_wr += bytes_wr;
    }

    total_wr = 0;
    while (total_wr < N){
        bytes_wr = write(sock_fd, &buffer[total_wr], N - total_wr);
        if (bytes_wr < 0){
            perror("Couldn't read message");
            exit(EXIT_FAILURE);
        }
        total_wr += bytes_wr;
    }

    total_wr = 0;
    while (total_wr < 4) {
        bytes_wr = read(sock_fd, &buf_len[total_wr], 4 - total_wr);
        if (bytes_wr < 0){
            perror("Couldn't read message");
            exit(EXIT_FAILURE);
        }
        total_wr += bytes_wr;
    }

    close(sock_fd); // is socket really done here?
    printf("# of printable characters: %u\n", ntohl(atoi(buf_len)));
    return 0;
}
