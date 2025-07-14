#include "string.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "netinet/in.h"
#include "sys/epoll.h"
#include "fcntl.h"
#include "unistd.h"
#include "queue.h"
#include "semaphore.h"
#include "time.h"

#define MAX_EVENTS 10
#define N_WORKERS 24

int setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl failed");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl failed");
        return -1;
    }

    return 0;
}

typedef struct WorkerCtx {
    size_t id;
    pthread_mutex_t* mut;
    sem_t* sem;
    Queue* q;
} WorkerCtx;

int process_client_fd(WorkerCtx* ctx, int client_fd) {
    size_t buf_size = 1024;
    char buf[buf_size];
    ssize_t n = recv(client_fd, buf, buf_size - 1, MSG_NOSIGNAL);
    if (n > 0) {
        buf[n] = '\0';
        printf("[worker %zu %zx] received %s\n", ctx->id, pthread_self(), buf);

        char res[100];
        sprintf(res, "hello from worker %zu", ctx->id);
        ssize_t n_send = send(client_fd, res, strlen(res), MSG_NOSIGNAL);
        if (n_send == 0) {
            fprintf(stderr, "failed to respond to client\n");
        }
    } else if (n == 0) {
        printf("client connection closed\n");
        close(client_fd);
    } else {
        fprintf(stderr, "recv failed\n");
        close(client_fd);
    }

    return 0;
}

void* process_request(void* worker_ctx) {
    WorkerCtx* ctx = (WorkerCtx*)worker_ctx;
    struct timespec wait_sec;
    wait_sec.tv_sec = 1;
    wait_sec.tv_nsec = 0;

    for (;;) {
        pthread_mutex_lock(ctx->mut);
        if (ctx->q->size == 0) {
            printf("[worker %zu %zx] waiting...\n", ctx->id, pthread_self());
            pthread_mutex_unlock(ctx->mut);
            sem_wait(ctx->sem);
            continue;
        }

        int client_fd = queue_pop(ctx->q);
        pthread_mutex_unlock(ctx->mut);

        // nanosleep(&wait_sec, NULL);
        process_client_fd(ctx, client_fd);
        printf("[worker %zu %zx] processed task\n", ctx->id, pthread_self());
    }
}

int main() {
    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
    sem_t sem;
    sem_init(&sem, 0, 0);

    Queue q;
    queue_init(&q);

    size_t n_workers = N_WORKERS;
    pthread_t workers[n_workers];
    WorkerCtx ctxs[n_workers];
    for (size_t i = 0; i < n_workers; ++i) {
        ctxs[i] = (WorkerCtx){.mut = &mut, .sem = &sem, .q = &q, .id = i + 1};
    }

    for (size_t i = 0; i < n_workers; ++i) {
        if (pthread_create(&workers[i], NULL, process_request, &ctxs[i]) ==
            -1) {
            perror("failed to create worker thread");
            exit(EXIT_FAILURE);
        }
    }

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

    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("failed to create socket");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) ==
        -1) {
        perror("failed to set socket option");
        exit(EXIT_FAILURE);
    }

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    size_t max_conn = 5;
    if (listen(listen_sock, max_conn) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll ctl failed");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll wait failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listen_sock) {
                conn_sock = accept(listen_sock, NULL, NULL);
                if (conn_sock == -1) {
                    perror("failed to accept connection");
                    exit(EXIT_FAILURE);
                }

                setnonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                    perror("epoll ctl failed");
                    exit(EXIT_FAILURE);
                }
            } else {
                pthread_mutex_lock(&mut);
                queue_add(&q, events[i].data.fd);
                sem_post(&sem);
                pthread_mutex_unlock(&mut);
            }
        }
    }

    for (size_t i = 0; i < n_workers; ++i) {
        if (pthread_join(workers[i], NULL) == -1) {
            fprintf(stderr, "failed to join worker thread");
        }
    }

    pthread_mutex_destroy(&mut);
    sem_destroy(&sem);
}