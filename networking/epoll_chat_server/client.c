#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "sys/socket.h"
#include "string.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "pthread.h"
#include "sys/ioctl.h"

#define IP "127.0.0.1"
#define PORT 10000

void* receive_message(void* sock) {
    int* s = sock;

    for (;;) {
        size_t buf_size = 1024;
        char buf[buf_size];
        memset(&buf, 0, buf_size);
        ssize_t nrecv = recv(*s, buf, buf_size, 0);
        if (nrecv > 0) {
            printf("%s\n", buf);
        } else if (nrecv == 0) {
            printf("lost connection\n");
            // we can try to reconnect
            break;
        } else {
            perror("recv");
        }
    }

    return NULL;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid usage, required one argument <name>\n");
        exit(EXIT_FAILURE);
    }

    char* name = argv[1];
    printf("using name: %s\n", name);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    printf("\033[%d;1H]", w.ws_row);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, IP, &addr.sin_addr) <= 0) {
        perror("invalid ip address");
        exit(EXIT_FAILURE);
    }

    int cs = socket(AF_INET, SOCK_STREAM, 0);
    if (cs == -1) {
        perror("socket create");
        exit(EXIT_FAILURE);
    }

    if (connect(cs, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("failed to connect");
        exit(EXIT_FAILURE);
    }

    ssize_t nsent = send(cs, name, strlen(name), 0);
    if (nsent < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    pthread_t receiver;
    if (pthread_create(&receiver, NULL, receive_message, (void*)&cs) == -1) {
        perror("failed to create receiver thread\n");
    }

    for (;;) {
        size_t buf_size = 1024;
        char buf[buf_size];
        memset(&buf, 0, sizeof(buf));

        printf("\033[%d;1H] msg: ", w.ws_row);
        scanf("%s", buf);

        ssize_t nsent = send(cs, buf, strlen(buf), 0);
        if (nsent < 0) {
            perror("send");
            continue;
        }
    }

    if (pthread_join(receiver, NULL) == -1) {
        perror("failed to join receiver thread\n");
    }
}