#include "arpa/inet.h"
#include "netinet/in.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "time.h"

int main() {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_port = htons(10000);
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sfd, 10) != 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct timespec wait_time_seconds;
    wait_time_seconds.tv_nsec = 0;
    wait_time_seconds.tv_sec = 1;

    while (true) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd == -1) {
            perror("client socket");
        }

        while (true) {
            char* msg = "message";
            ssize_t n = send(cfd, msg, strlen(msg), MSG_NOSIGNAL);
            nanosleep(&wait_time_seconds, NULL);
        }
    }
}
