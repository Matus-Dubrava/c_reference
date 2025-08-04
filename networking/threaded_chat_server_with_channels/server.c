#define _GNU_SOURCE
#include "stdbool.h"
#include "semaphore.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "unistd.h"

#define MAX_CONNS 24
#define BUF_SIZE 1024

typedef struct Channels {
    int* items;
    size_t cap;
    size_t size;
} Channels;

typedef struct Conn {
    int sock;
    Channels* channels;
} Conn;

typedef struct Event {
    Conn* conn;
    char* msg;
    int channel;
} Event;

Event* event_create(Conn* conn, char* msg, int channel) {
    Event* ev = malloc(sizeof(Event));
    if (!ev) {
        perror("malloc event");
        return NULL;
    }

    ev->conn = conn;
    ev->msg = strdup(msg);
    ev->channel = channel;
    return ev;
}

void event_free(Event* ev) {
    free(ev->msg);
    free(ev);
}

void event_print(Event* ev) {
    printf("[%d] channel: %d, msg: %s\n", ev->conn->sock, ev->channel, ev->msg);
}

typedef struct Queue {
    Event** events;
    size_t size;
    size_t cap;
} Queue;

Queue* queue_create(size_t cap) {
    Queue* q = malloc(sizeof(Queue));
    if (!q) {
        perror("malloc queue");
        return NULL;
    }

    Event** events = malloc(cap * sizeof(Event*));
    if (!events) {
        perror("malloc eventst");
        free(q);
        return NULL;
    }

    q->events = events;
    q->cap = cap;
    q->size = 0;
    return q;
}

void queue_free(Queue* q) {
    for (size_t i = 0; i < q->size; ++i) {
        event_free(q->events[i]);
    }

    free(q->events);
    free(q);
}

int parse_message(char* msg, int* channel_out, char** message_out) {
    size_t msg_len = strlen(msg);

    size_t i = 0;
    char buf[BUF_SIZE];
    memset(buf, 0, sizeof(buf));
    while (i < msg_len && *msg != ' ') {
        buf[i++] = *msg++;
    }

    int channel = atoi(buf);
    if (channel == 0) {
        fprintf(stderr, "failed to parse message\n");
        return -1;
    }

    *channel_out = channel;
    *message_out = ++msg;
    return 0;
}

int queue_add(Queue* q, Conn* conn, char* msg) {
    if (q->size >= q->cap) {
        size_t new_cap = q->cap * 2;
        Event** new_events = realloc(q->events, new_cap * sizeof(Event*));
        if (!new_events) {
            perror("realloc events");
            queue_free(q);
            return -1;
        }

        q->events = new_events;
        q->cap = new_cap;
    }

    int channel = 0;
    char* parsed_msg = malloc(BUF_SIZE * sizeof(char));
    if (!parsed_msg) {
        perror("malloc parsed_msg");
        return -1;
    }
    parsed_msg[0] = '\0';

    if (parse_message(msg, &channel, &parsed_msg) == -1) {
        printf("MSG - channel: %d, msg: %s\n", channel, parsed_msg);
        return -1;
    }

    Event* ev = event_create(conn, parsed_msg, channel);
    if (!ev) {
        return -1;
    }

    q->events[q->size++] = ev;
    return 0;
}

Event* queue_peek(Queue* q) {
    if (q->size > 0) {
        return q->events[q->size - 1];
    }

    return NULL;
}

Channels* channels_create(size_t cap) {
    Channels* channels = malloc(sizeof(Channels));
    if (!channels) {
        perror("malloc channels");
        return NULL;
    }

    int* items = malloc(cap * sizeof(int));
    if (!items) {
        perror("malloc channels:items");
        free(channels);
        return NULL;
    }

    channels->items = items;
    channels->size = 0;
    channels->cap = cap;
    return channels;
}

void channels_free(Channels* channels) {
    free(channels->items);
    free(channels);
}

int channels_add(Channels* channels, int channel) {
    if (channels->size >= channels->cap) {
        size_t new_cap = channels->cap * 2;
        int* new_items = realloc(channels->items, new_cap * sizeof(int));
        if (!new_items) {
            perror("realloc channels:items");
            channels_free(channels);
            return -1;
        }
        channels->cap = new_cap;
        channels->items = new_items;
    }

    channels->items[channels->size++] = channel;
    return 0;
}

Conn* conn_create(int sock) {
    Conn* c = malloc(sizeof(Conn));
    if (!c) {
        perror("malloc connection");
        return NULL;
    }

    Channels* channels = channels_create(100);
    if (!channels) {
        free(c);
        return NULL;
    }

    c->sock = sock;
    c->channels = channels;
    return c;
}

void conn_free(Conn* conn) {
    channels_free(conn->channels);
    free(conn);
}

typedef struct Conns {
    Conn** items;
    size_t size;
    size_t cap;
} Conns;

void conns_free(Conns* conns) {
    for (size_t i = 0; i < conns->size; ++i) {
        conn_free(conns->items[i]);
    }

    free(conns);
}

Conns* conns_create(size_t cap) {
    Conns* conns = malloc(sizeof(Conns));
    if (!conns) {
        perror("malloc connections");
        return NULL;
    }

    Conn** items = malloc(cap * sizeof(Conn*));
    if (!items) {
        perror("malloc itemst");
        free(conns);
        return NULL;
    }

    conns->items = items;
    conns->cap = cap;
    conns->size = 0;
    return conns;
}

Conn* conns_add(Conns* conns, int sock) {
    if (conns->size >= conns->cap) {
        size_t new_cap = conns->cap * 2;
        Conn** new_conns = realloc(conns->items, new_cap * sizeof(Conn*));
        if (!new_conns) {
            perror("realloc items");
            conns_free(conns);
            return NULL;
        }

        conns->items = new_conns;
        conns->cap = new_cap;
    }

    Conn* conn = conn_create(sock);
    if (!conn) {
        return NULL;
    }

    conns->items[conns->size++] = conn;
    return conn;
}

void conns_remove(Conns* conns, int sock) {
    for (size_t i = 0; i < conns->size; ++i) {
        if (conns->items[i]->sock == sock) {
            conns->items[i] = conns->items[--conns->size];
            break;
        }
    }
}

typedef struct Manager {
    Conns* conns;
    size_t q_idx;
    Queue* q;
} Manager;

Manager* manager_create() {
    Manager* manager = malloc(sizeof(Manager));
    if (!manager) {
        perror("malloc manager");
        return NULL;
    }

    Queue* q = queue_create(1000);
    if (!q) {
        free(manager);
        return NULL;
    }

    Conns* conns = conns_create(1000);
    if (!conns) {
        free(manager);
        queue_free(q);
        return NULL;
    }

    manager->q = q;
    manager->conns = conns;
    manager->q_idx = 0;
    return manager;
}

void manager_free(Manager* manager) {
    queue_free(manager->q);
    conns_free(manager->conns);
    free(manager);
}

typedef struct ReceiverCtx {
    Conn* conn;
    pthread_mutex_t* mut;
    Manager* manager;
    sem_t* sem;
} ReceiverCtx;

ReceiverCtx* receiver_ctx_create(Conn* conn,
                                 pthread_mutex_t* mut,
                                 Manager* manager,
                                 sem_t* sem) {
    ReceiverCtx* ctx = malloc(sizeof(ReceiverCtx));
    if (!ctx) {
        perror("malloc receiver ctx");
        return NULL;
    }

    ctx->manager = manager;
    ctx->mut = mut;
    ctx->conn = conn;
    ctx->sem = sem;
    return ctx;
}

void receiver_ctx_free(ReceiverCtx* ctx) {
    free(ctx);
}

void* receiver_process(void* receiver_ctx) {
    ReceiverCtx* ctx = (ReceiverCtx*)receiver_ctx;

    for (;;) {
        char buf[BUF_SIZE];
        ssize_t nrecv = recv(ctx->conn->sock, buf, BUF_SIZE - 1, 0);
        if (nrecv > 0) {
            buf[nrecv] = '\0';

            pthread_mutex_lock(ctx->mut);
            if (queue_add(ctx->manager->q, ctx->conn, buf) == -1) {
                close(ctx->conn->sock);
                conns_remove(ctx->manager->conns, ctx->conn->sock);
                pthread_mutex_unlock(ctx->mut);
                break;
            }
            sem_post(ctx->sem);
            pthread_mutex_unlock(ctx->mut);
        } else if (nrecv == 0) {
            printf("client disconnected\n");
            close(ctx->conn->sock);
            conns_remove(ctx->manager->conns, ctx->conn->sock);
            break;
        } else {
            perror("recv");
            close(ctx->conn->sock);
            conns_remove(ctx->manager->conns, ctx->conn->sock);
            break;
        }
    }

    return NULL;
}

typedef struct ManagerCtx {
    Manager* manager;
    pthread_mutex_t* mut;
    sem_t* sem;
} ManagerCtx;

void* manager_process(void* manager_ctx) {
    ManagerCtx* ctx = (ManagerCtx*)manager_ctx;

    for (;;) {
        sem_wait(ctx->sem);
        pthread_mutex_lock(ctx->mut);
        Event* ev = queue_peek(ctx->manager->q);
        pthread_mutex_unlock(ctx->mut);

        event_print(ev);
        char msg[BUF_SIZE + 100];

        if (strncmp(ev->msg, "$reg", 4) == 0) {
            if (channels_add(ev->conn->channels, ev->channel) == -1) {
                fprintf(stderr, "failed to register channel\n");
            } else {
                printf("registered channel\n");
            }
        } else {
            sprintf(msg, "[%d] %s", ev->conn->sock, ev->msg);
            for (size_t i = 0; i < ctx->manager->conns->size; ++i) {
                printf("conn: %d: ", ctx->manager->conns->items[i]->sock);
                if (ev->conn->sock != ctx->manager->conns->items[i]->sock) {
                    for (size_t j = 0;
                         j < ctx->manager->conns->items[i]->channels->size;
                         ++j) {
                        printf(
                            "%d ",
                            ctx->manager->conns->items[i]->channels->items[j]);
                        if (ctx->manager->conns->items[i]->channels->items[j] ==
                            ev->channel) {
                            ssize_t nsent =
                                send(ctx->manager->conns->items[i]->sock, msg,
                                     strlen(msg), 0);

                            if (nsent < 0) {
                                perror("manager send");
                            }

                            break;
                        }
                    }
                }
                printf("\n");
            }
        }
    }
}

int main() {
    Manager* manager = manager_create();
    if (!manager) {
        exit(EXIT_FAILURE);
    }

    pthread_t receivers[MAX_CONNS];
    ReceiverCtx* receiver_ctxs[MAX_CONNS];
    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
    sem_t sem;
    sem_init(&sem, 0, 0);

    pthread_t manager_t;
    ManagerCtx manager_ctx = {.manager = manager, .sem = &sem, .mut = &mut};

    if (pthread_create(&manager_t, NULL, manager_process, &manager_ctx) == -1) {
        perror("manager pthread create");
        exit(EXIT_FAILURE);
    }

    char* ip = "127.0.0.1";
    int port = 8000;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("invalid IP address");
        exit(EXIT_FAILURE);
    }

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == -1) {
        perror("linstening socket");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1) {
        perror("setsockopt REUSEADDR");
        exit(EXIT_FAILURE);
    }

    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(ls, MAX_CONNS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    size_t receivers_size = 0;
    for (;;) {
        int cs = accept(ls, NULL, NULL);
        if (cs == -1) {
            perror("connection socket");
            continue;
        }

        Conn* conn = conns_add(manager->conns, cs);
        if (!conn) {
            continue;
        }

        ReceiverCtx* ctx = receiver_ctx_create(conn, &mut, manager, &sem);
        if (!ctx) {
            continue;
        }

        if (pthread_create(&receivers[receivers_size], NULL, receiver_process,
                           ctx) == -1) {
            perror("receiver pthread create");
            receiver_ctx_free(ctx);
            continue;
        }

        receiver_ctxs[receivers_size++] = ctx;
    }

    pthread_mutex_destroy(&mut);
    manager_free(manager);
    sem_destroy(&sem);

    for (size_t i = 0; i < receivers_size; ++i) {
        receiver_ctx_free(receiver_ctxs[i]);
    }
}