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

#define PCC_LEN 96
#define READ 'r'
#define WRITE 'w'

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

int wr(char wr, char *buffer, uint32_t max_bytes, char *err_msg){
    ssize_t total_wr = 0, bytes_wr = 0;
    while (total_wr < max_bytes) {
        if (wr == WRITE) bytes_wr = write(conn_fd, buffer + total_wr, max_bytes - total_wr);
        else if (wr == READ) bytes_wr = read(conn_fd, buffer + total_wr, max_bytes - total_wr);
        if (bytes_wr < 0) {
            perror(err_msg);
            exit(EXIT_FAILURE);
        }
        if (bytes_wr == 0 && total_wr != max_bytes) {
            fprintf(stderr, "client was unexpectedly killed. what a shame.\n");
            fprintf(stderr,"server still able to accept new clients though");
            close(conn_fd);
            conn_fd = -1;
            return -1;
        }
        total_wr += bytes_wr;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int sock_fd = -1;
    uint32_t N, port, buf_N;
    char *buffer;
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
        if (wr(READ, (char *) &buf_N, 4, "Couldn't read message length") < 0) continue;

        // convert N to host form and allocate memory for data
        N = ntohl(buf_N);
        buffer = (char *) malloc(N * sizeof(char));

        if (wr(READ, buffer, N, "Couldn't read message") < 0) continue;

        // calculate C and count readable chars in pcc_buff
        counter = 0;
        for (int i = 0; i < PCC_LEN; i++) pcc_current[i] = 0;
        for (int i = 0; i < N; i++) {
            if (32 <= buffer[i] && buffer[i] <= 126) {
                counter++;
                pcc_current[(int) (buffer[i] - 32)]++;
            }
        }
        buf_N = htonl(counter);

        if (wr(WRITE, (char *) &buf_N, 4, "Couldn't write to client") < 0) continue;

        for (int i = 0; i < 95; i++) pcc_total[i] += pcc_current[i];
        close(conn_fd);
        conn_fd = -1;
    }

    terminate();
}