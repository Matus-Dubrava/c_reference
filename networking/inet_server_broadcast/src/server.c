#include "arpa/inet.h"
#include "netinet/in.h"
#include "pthread.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "time.h"

void* increment_counter(void* counter) {
    struct timespec wait_time;
    wait_time.tv_sec = 1;
    wait_time.tv_nsec = 0;

    while (true) {
        *(int*)counter += 1;

        nanosleep(&wait_time, NULL);
    }

    return NULL;
}

typedef struct HandlerCtx {
    int cfd;
    struct timespec* wait_time;
    size_t* counter;
} HandlerCtx;

void* handle_connection(void* handler_ctx) {
    HandlerCtx* ctx = (HandlerCtx*)handler_ctx;

    while (true) {
        char buf[1024];
        sprintf(buf, "%zu", *ctx->counter);

        ssize_t n = send(ctx->cfd, buf, strlen(buf), MSG_NOSIGNAL);
        if (n == -1) {
            break;
        }
        nanosleep(ctx->wait_time, NULL);
    }

    return NULL;
}

int main() {
    size_t counter = 0;
    pthread_t counter_t;
    if (pthread_create(&counter_t, NULL, increment_counter, (void*)&counter) !=
        0) {
        perror("counter increment thread create");
        exit(EXIT_FAILURE);
    }

    size_t n_clients = 0;
    size_t max_clients = 10;
    pthread_t handler_ts[max_clients];
    HandlerCtx handler_ctxs[max_clients];

    char* IP = "127.0.0.1";
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_port = htons(10000);
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, IP, &addr.sin_addr) <= 0) {
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

    if (listen(sfd, max_clients) != 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct timespec wait_time;
    wait_time.tv_nsec = 0;
    wait_time.tv_sec = 1;

    while (true) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd == -1) {
            perror("client socket");
        }
        printf("client connected\n");

        if (n_clients < max_clients) {
            handler_ctxs[n_clients] = (HandlerCtx){
                .cfd = cfd, .counter = &counter, .wait_time = &wait_time};

            if (pthread_create(&handler_ts[n_clients], NULL, handle_connection,
                               &handler_ctxs[n_clients]) == -1) {
                perror("handler thread create");
            }
            n_clients++;
        }
    }

    for (size_t i = 0; i < n_clients; ++i) {
        if (pthread_join(handler_ts[i], NULL) != 0) {
            perror("handler thread join");
        }
    }

    if (pthread_join(counter_t, NULL) != 0) {
        perror("counter thread join");
    }
}
