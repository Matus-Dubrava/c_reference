#define _GNU_SOURCE
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define BUF_SIZE 500

int main() {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    ssize_t nread;
    char buf[BUF_SIZE];

    // char* host = "google.com";
    char* host = "localhost";
    char* port = "80";

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    s = getaddrinfo(host, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)rp->ai_addr;
        void* addr = &(ipv4->sin_addr);

        char buf[BUF_SIZE];
        inet_ntop(AF_INET, addr, buf, sizeof(buf));

        printf("addrinfo: %s, family: %d\n", buf, rp->ai_family);
    }

    (void)buf;
    (void)rp;
    (void)sfd;
    (void)nread;
}