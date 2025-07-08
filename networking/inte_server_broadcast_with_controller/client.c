#include "arpa/inet.h"
#include "config.h"
#include "messages.h"
#include "netinet/in.h"
#include "pthread.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "time.h"

int main() {
    size_t max_attempts = 5;
    size_t attempt = 0;
    struct timespec wait_time;
    wait_time.tv_nsec = 0;
    wait_time.tv_sec = 3;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_MAIN);
    if (inet_pton(AF_INET, IP_ADDR, &addr.sin_addr) == -1) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    while (true) {
        bool failed = false;

        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1) {
            perror("failed to create server socket");
            exit(EXIT_FAILURE);
        }

        if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("failed to connect to server");
            failed = true;
        }

        while (true && !failed) {
            char buf[MAX_BUFSIZE];

            ssize_t n = recv(client_fd, buf, MAX_BUFSIZE, MSG_NOSIGNAL);
            if (n > 0) {
                attempt = 0;
                buf[n] = '\0';
                if (strncmp(buf, C_MSG_EXIT, 4) == 0) {
                    printf("received: shutdown signal\n");
                    goto exit;
                }
                printf("received: %s\n", buf);
            } else {
                break;
            }
        }

        if (attempt >= max_attempts) {
            break;
        } else {
            attempt++;
        }
        printf(
            "connection terminated; retrying in %zu seconds (attempt "
            "%zu/%zu)\n",
            wait_time.tv_sec, attempt, max_attempts);
        nanosleep(&wait_time, NULL);
    }
exit:
    return 0;
}