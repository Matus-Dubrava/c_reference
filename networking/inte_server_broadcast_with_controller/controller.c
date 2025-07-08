#include "arpa/inet.h"
#include "config.h"
#include "netinet/in.h"
#include "pthread.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "time.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "expected 1 argument; got %d", argc);
        exit(EXIT_FAILURE);
    }

    struct timespec wait_time;
    wait_time.tv_nsec = 0;
    wait_time.tv_sec = 3;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_CONTROLLER);
    if (inet_pton(AF_INET, IP_ADDR, &addr.sin_addr) == -1) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("failed to create socket");
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("failed to connect to server");
        exit(EXIT_FAILURE);
    }

    ssize_t n = send(client_fd, argv[1], strlen(argv[1]), MSG_NOSIGNAL);
    printf("send %zu bytes to server\n", n);
}
