#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "netinet/in.h"
#include "unistd.h"
#include "string.h"
#include "pthread.h"

typedef struct WorkerCtx {
    int client_fd;
} WorkerCtx;

void* handle_connection(void* worker_ctx) {
    WorkerCtx* ctx = (WorkerCtx*)worker_ctx;

    while (true) {
        size_t buf_size = 1024;
        char buf[buf_size];
        ssize_t n = recv(ctx->client_fd, buf, buf_size - 1, MSG_NOSIGNAL);
        if (n > 0) {
            buf[n] = '\0';
            printf("[worker %zx] received: %s\n", pthread_self(), buf);
        } else {
            printf("client disconnected\n");
            break;
        }
    }

    free(ctx);
    return NULL;
}

int main() {
    char* ip = "127.0.0.1";
    int port = 10000;
    struct sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("failed to create socket");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) ==
        -1) {
        perror("failed to set socket options");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("failed to bind");
        exit(EXIT_FAILURE);
    }

    size_t n_connections = 5;
    if (listen(server_fd, n_connections) == -1) {
        perror("failed to start listening");
        exit(EXIT_FAILURE);
    }

    size_t max_workers = 5;
    size_t n_workers = 0;
    pthread_t workers[max_workers];

    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("failed to accept client connection");
        }

        if (n_workers < max_workers) {
            WorkerCtx* ctx = malloc(sizeof(WorkerCtx));
            if (!ctx) {
                perror("failed to allocate memory for worker ctx");
                break;
            }
            ctx->client_fd = client_fd;
            if (pthread_create(&workers[n_workers++], NULL, handle_connection,
                               (void*)ctx) == -1) {
                perror("failed to create worker thread");
            }
        } else {
            printf("refusing connection; no worker available\n");
            char* resp = "connection refused: server busy\r\n";
            ssize_t n = send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
            if (n == 0) {
                fprintf(stderr, "failed to send message to client\n");
            }
            if (close(client_fd) == -1) {
                perror("failed to close client connection");
            }
        }
    }

    for (size_t i = 0; i < n_workers; ++i) {
        if (pthread_join(workers[i], NULL) == -1) {
            perror("worker thread failed to join");
        }
    }

    if (close(server_fd) == -1) {
        perror("failed to close server fd");
    }
}