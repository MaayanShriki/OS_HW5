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

#define PCC_LEN 96

int conn_fd = -1, killed = 0;
int pcc_total[PCC_LEN] = {0};


void terminate() {
    for (int i = 0; i < PCC_LEN; i++)
        printf("char '%c' : %u times\n", (i + 32), pcc_total[i]);
    exit(EXIT_SUCCESS);
}

void sigint_handler() {
    if (conn_fd == -1) terminate();
    else killed = 1;
}

int main(int argc, char *argv[]) {
    int sock_fd = -1;
    uint32_t N, port;
    char *buffer, buf_len[4];
    ssize_t total_wr, bytes_wr = 0;
    int pcc_current[PCC_LEN] = {0}, counter;
    if (argc != 2) {
        perror("exactly 1 input is required.");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);

    struct sigaction sigint;
    sigint.sa_flags = SA_RESTART;
    sigint.sa_handler = &sigint_handler;
    sigemptyset(&sigint.sa_mask);
    if (sigaction(SIGINT, &sigint, 0) != 0) {
        perror("Signal handle registration failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Couldn't create socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("Couldn't set reusable port");
        exit(EXIT_FAILURE);
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (struct sockaddr *) &server, sizeof(server)) != 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sock_fd, 10) != 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }


    while (!killed) {
        conn_fd = accept(sock_fd, NULL, NULL);
        if (conn_fd < 0) {
            perror("couldn't accept connection");
            exit(EXIT_FAILURE);
        }
        // read N from client (4 bytes)
        total_wr = 0;
        while (total_wr < 4) {
            bytes_wr = read(conn_fd, &buf_len[total_wr], 4 - total_wr);
            if (bytes_wr < 0) {
                perror("Couldn't read message length");
                exit(EXIT_FAILURE);
            }
            if (bytes_wr == 0 && total_wr != 4) {
                printf("client was unexpectedly killed. what a shame.");
                printf("server still able to accept new clients though");
                close(conn_fd);
                conn_fd = -1;
                break;
            }
            total_wr += bytes_wr;
        }
        if (bytes_wr == 0 && total_wr != 4) continue;

        // convert N to host form and allocate memory for data
        N = ntohl(atoi(buf_len));
        buffer = (char *) malloc(N * sizeof(char));

        total_wr = 0;
        while (total_wr < N) {
            bytes_wr = read(conn_fd, &buffer[total_wr], N - total_wr);
            if (bytes_wr < 0) {
                perror("Couldn't read message");
                exit(EXIT_FAILURE);
            }
            if (bytes_wr == 0 && total_wr != N) {
                printf("client was unexpectedly killed. what a shame.");
                printf("server still able to accept new clients though");
                close(conn_fd);
                conn_fd = -1;
                break;
            }
            total_wr += bytes_wr;
        }
        if (bytes_wr == 0 && total_wr != N) continue;


        // calculate C and count readable chars in pcc_buff
        counter = 0;
        for (int i = 0; i < PCC_LEN; i++) pcc_current[i] = 0;
        for (int i = 0; i < N; i++) {
            if (32 <= buffer[i] && buffer[i] <= 126) {
                counter++;
                pcc_current[(int) (buffer[i] - 32)]++;
            }
        }
        counter = htons(counter);
        buf_len[0] = (char) (counter & 0xff);
        buf_len[1] = (char) ((counter >> 8) & 0xff);
        buf_len[2] = (char) ((counter >> 16) & 0xff);
        buf_len[3] = (char) ((counter >> 24) & 0xff);

        total_wr = 0;
        while (total_wr < 4) {
            bytes_wr = write(conn_fd, &buf_len[total_wr], 4 - total_wr);
            if (bytes_wr < 0) {
                perror("Couldn't write to client");
                exit(EXIT_FAILURE);
            }
            if (bytes_wr == 0 && total_wr != 4) {
                printf("client was unexpectedly killed. what a shame.");
                printf("server still able to accept new clients though");
                close(conn_fd);
                conn_fd = -1;
                break;
            }
            total_wr += bytes_wr;
        }
        if (bytes_wr == 0 && total_wr != 4) continue;

        for (int i = 0; i < 95; i++) pcc_total[i] += pcc_current[i];
        close(conn_fd);
        conn_fd = -1;
    }

    terminate();
}