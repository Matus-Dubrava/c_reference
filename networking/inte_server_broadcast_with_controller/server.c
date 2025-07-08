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

typedef struct WorkerCtx {
    size_t max_connections;
    size_t* n_connections;
    int* client_fds;
    pthread_mutex_t* mut;
    int server_fd;
    int* controller_signal;
} WorkerCtx;

void* send_response(void* worker_ctx) {
    WorkerCtx* ctx = (WorkerCtx*)worker_ctx;
    struct timespec wait_time;
    wait_time.tv_nsec = 0;
    wait_time.tv_sec = 1;

    size_t counter = 0;

    while (true) {
        char buf[MAX_BUFSIZE];
        if (*ctx->controller_signal == C_EXIT) {
            strncpy(buf, C_MSG_EXIT, 4);
        } else if (*ctx->controller_signal == C_RESET) {
            counter = 0;
            sprintf(buf, "%zu", counter++);
        } else {
            sprintf(buf, "%zu", counter++);
        }
        *ctx->controller_signal = C_NO_SIGNAL;

        pthread_mutex_lock(ctx->mut);
        for (size_t i = 0; i < *ctx->n_connections; ++i) {
            ssize_t n =
                send(ctx->client_fds[i], buf, strlen(buf), MSG_NOSIGNAL);
            printf("send %zu bytes to worker %zu (%d)\n", n, i,
                   ctx->client_fds[i]);
        }
        pthread_mutex_unlock(ctx->mut);
        nanosleep(&wait_time, NULL);
    }
}

void* accept_workers(void* worker_ctx) {
    WorkerCtx* ctx = (WorkerCtx*)worker_ctx;

    while (true) {
        printf("waiting for clients\n");
        int client_fd = accept(ctx->server_fd, NULL, NULL);
        printf("connection accepted; n_connections %zu\n", *ctx->n_connections);
        if (client_fd == -1) {
            perror("failed to accept client connection");
            continue;
        }

        if (*ctx->n_connections < ctx->max_connections) {
            pthread_mutex_lock(ctx->mut);
            ctx->client_fds[*ctx->n_connections] = client_fd;
            *ctx->n_connections += 1;
            pthread_mutex_unlock(ctx->mut);
        } else {
            printf("refused connection; no connection available\n");
        }
    }
}

typedef struct ControllerCtx {
    int controller_fd;
    char* controller_msg;
    int* controller_signal;
} ControllerCtx;

void* accept_controller(void* controller_ctx) {
    ControllerCtx* ctx = (ControllerCtx*)controller_ctx;

    while (true) {
        printf("waiting for controller\n");
        int client_fd = accept(ctx->controller_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("failed to accept controller connection");
            continue;
        }

        char buf[MAX_BUFSIZE];
        ssize_t n = recv(client_fd, buf, MAX_BUFSIZE - 1, MSG_NOSIGNAL);
        if (n > 0) {
            buf[n] = '\0';
            printf("received from controller: %s\n", buf);

            // handle controller messages
            if (strncmp(buf, C_MSG_EXIT, 4) == 0) {
                *ctx->controller_signal = C_EXIT;
            } else if (strncmp(buf, C_MSG_RESET, strlen(C_MSG_RESET)) == 0) {
                *ctx->controller_signal = C_RESET;
            } else {
                fprintf(stderr, "unknown controller signal: %s\n", buf);
            }
        } else {
            printf("controller disconnected\n");
        }
    }
}

int create_server_listener(size_t max_connections) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_MAIN);
    if (inet_pton(AF_INET, IP_ADDR, &addr.sin_addr) == -1) {
        perror("invalid IP address");
        return -1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("failed to create server socket");
        return -1;
    }

    int one = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) ==
        -1) {
        perror("failed to set socket option REUSEADDR");
        return -1;
    }

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("failed to bind the socket");
        return -1;
    }

    if (listen(server_fd, max_connections) == -1) {
        perror("failed to start listening");
        return -1;
    }

    return server_fd;
}

int create_controller_listener() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_CONTROLLER);
    if (inet_pton(AF_INET, IP_ADDR, &addr.sin_addr) == -1) {
        perror("invalid IP address");
        return -1;
    }

    int controller_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (controller_fd == -1) {
        perror("failed to create server socket");
        return -1;
    }

    int one = 1;
    if (setsockopt(controller_fd, SOL_SOCKET, SO_REUSEADDR, &one,
                   sizeof(one)) == -1) {
        perror("failed to set socket option REUSEADDR");
        return -1;
    }

    if (bind(controller_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("failed to bind the socket");
        return -1;
    }

    if (listen(controller_fd, 1) == -1) {
        perror("failed to start listening");
        return -1;
    }

    return controller_fd;
}

int main() {
    int controller_signal = C_NO_SIGNAL;

    size_t max_connections = 5;
    size_t n_connections = 0;
    pthread_t worker;
    int client_fds[max_connections];
    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

    struct timespec wait_time;
    wait_time.tv_nsec = 0;
    wait_time.tv_sec = 1;

    int server_fd = create_server_listener(max_connections);
    if (server_fd == -1) {
        exit(EXIT_FAILURE);
    }

    int controller_fd = create_controller_listener();
    if (controller_fd == -1) {
        exit(EXIT_FAILURE);
    }

    // create worker thread
    WorkerCtx worker_ctx = {.max_connections = max_connections,
                            .n_connections = &n_connections,
                            .client_fds = client_fds,
                            .mut = &mut,
                            .server_fd = server_fd,
                            .controller_signal = &controller_signal};

    if (pthread_create(&worker, NULL, send_response, &worker_ctx) == -1) {
        perror("failed to create worker thread");
        exit(EXIT_FAILURE);
    }

    // create worker listener thread
    pthread_t worker_listener;
    if (pthread_create(&worker_listener, NULL, accept_workers, &worker_ctx) ==
        -1) {
        perror("failed to create worker listener thread");
        exit(EXIT_FAILURE);
    }

    // controller thread
    pthread_t controller;
    ControllerCtx controller_ctx = {.controller_fd = controller_fd,
                                    .controller_signal = &controller_signal};
    if (pthread_create(&controller, NULL, accept_controller, &controller_ctx) ==
        -1) {
        perror("failed to create controller thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(worker, NULL) == -1) {
        perror("worker thread failed to join");
    }

    if (pthread_join(worker_listener, NULL) == -1) {
        perror("worker listener failed to join");
    }

    if (pthread_join(controller, NULL) == -1) {
        perror("controller failed to join");
    }

    pthread_mutex_destroy(&mut);
}
