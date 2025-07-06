#include "arpa/inet.h"
#include "netinet/in.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"

int main() {
    char* IP = "127.0.0.1";
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_port = htons(10000);
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, IP, &addr.sin_addr) == -1) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (connect(cfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("failed to connect to server");
    }

    while (true) {
        size_t buf_size = 1024;
        char buf[buf_size];
        ssize_t n = recv(cfd, buf, buf_size, MSG_NOSIGNAL);
        if (n > 0) {
            buf[n] = '\0';
            printf("received: %s\n", buf);
        } else {
            printf("nothing\n");
        }
    }
}