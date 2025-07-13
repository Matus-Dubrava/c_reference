#include "sys/socket.h"
#include "stdbool.h"
#include "time.h"
#include "arpa/inet.h"
#include "stdlib.h"
#include "stdio.h"
#include "netinet/in.h"
#include "string.h"

int main() {
    struct timespec wait;
    wait.tv_nsec = 0;
    wait.tv_sec = 1;

    size_t max_attempts = 5;
    size_t attempt = 0;
    struct timespec attempt_wait;
    attempt_wait.tv_nsec = 0;
    attempt_wait.tv_sec = 3;

    char* ip = "127.0.0.1";
    int port = 10000;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    while (true) {
        bool is_ok = true;

        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1) {
            perror("failed to create socket");
            exit(EXIT_FAILURE);
        }

        if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("failed to connect to server");
            is_ok = false;
        }

        size_t counter = 0;
        while (is_ok) {
            size_t buf_size = 1024;
            char buf[buf_size];
            sprintf(buf, "message %zu\n", counter++);

            ssize_t n = send(client_fd, buf, strlen(buf), MSG_NOSIGNAL);
            if (n > 0) {
                printf("sent %zu bytes\n", n);
            } else {
                fprintf(stderr, "failed to send message\n");
                is_ok = false;
                break;
            }

            n = recv(client_fd, buf, buf_size, MSG_NOSIGNAL);
            if (n > 0) {
                printf("received: %s\n", buf);
            } else {
                fprintf(stderr, "connection broken\n");
                is_ok = false;
                break;
            }

            attempt = 0;
            nanosleep(&wait, NULL);
        }

        if (!is_ok) {
            if (attempt <= max_attempts) {
                printf("retrying in %zu seconds (%zu/%zu)\n",
                       attempt_wait.tv_sec, attempt, max_attempts);
                attempt++;

                nanosleep(&attempt_wait, NULL);
            } else {
                fprintf(stderr, "failed to reconnect after %zu attempts\n",
                        attempt);
                break;
            }
        }
    }
}