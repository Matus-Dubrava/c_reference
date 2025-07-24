// channle based communication between clients
// messages are broadcasted within a channel
// poor design:
//   - blocking sockets
//   - one thread per connection
//   - connections are not being reclaimed

#include "stdio.h"
#include "time.h"
#include "stdlib.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "netinet/in.h"
#include "unistd.h"
#include "string.h"
#include "pthread.h"

#define MAX_CONN 1000
#define NWORKERS 24

typedef struct conn_t {
    int sock;
    size_t channel_id;
} conn_t;

typedef struct Connections {
    conn_t** items;
    size_t capacity;
    size_t len;
} Connections;

typedef struct worker_ctx_t {
    size_t id;
    Connections* conns;
} worker_ctx_t;

Connections* create_connections(size_t capacity) {
    Connections* conns = malloc(sizeof(Connections));
    if (!conns) {
        perror("malloc connections");
        return NULL;
    }

    conn_t** items = malloc(capacity * sizeof(conn_t*));
    if (!items) {
        free(conns);
        perror("malloc conn. items");
        return NULL;
    }

    conns->items = items;
    conns->capacity = capacity;
    conns->len = 0;
    return conns;
}

void conn_destroy(conn_t* conn) {
    free(conn);
}

void connections_destroy(Connections* conns) {
    for (size_t i = 0; i < conns->len; ++i) {
        conn_destroy(conns->items[i]);
    }
    free(conns);
}

void conn_display(conn_t* conn) {
    printf("conn(id=%zu, sock=%d)\n", conn->channel_id, conn->sock);
}

int register_connection(int sock, char* channel_id_str, Connections* conns) {
    printf("recv: %s\n", channel_id_str);

    conn_t* conn = malloc(sizeof(conn_t));
    if (!conn) {
        perror("malloc conn");
        return -1;
    }

    int channel_id = atoi(channel_id_str);
    if (channel_id == 0) {
        fprintf(stderr, "failed to convert %s to integer\n", channel_id_str);
        return -1;
    }

    conn->channel_id = channel_id;
    conn->sock = sock;

    if (conns->len < conns->capacity) {
        conns->items[conns->len++] = conn;
        printf("registered connection: ");
        conn_display(conn);
    } else {
        fprintf(stderr, "failed to register new connection; full capacity\n");
        return -1;
    }

    return 0;
}

void channel_broadcast(int sock,
                       Connections* conns,
                       char* msg,
                       size_t channel_id) {
    for (size_t i = 0; i < conns->len; ++i) {
        if (conns->items[i]->sock != sock &&
            conns->items[i]->channel_id == channel_id) {
            ssize_t nsent = send(conns->items[i]->sock, msg, strlen(msg), 0);
            if (nsent < 0) {
                perror("broadcast");
            }
        }
    }
}

void* handle_connection(void* worker_ctx) {
    worker_ctx_t* ctx = (worker_ctx_t*)worker_ctx;

    struct timespec wait;
    wait.tv_nsec = 0;
    wait.tv_sec = 1;

    // wait till the corresponding connection is active
    for (;;) {
        if (ctx->conns->items[ctx->id] == 0) {
            nanosleep(&wait, NULL);
        } else {
            printf("[%zx] waking up, connection %zu is active\n",
                   pthread_self(), ctx->id);
            break;
        }
    }

    for (;;) {
        size_t buf_size = 1024;
        char buf[buf_size];
        ssize_t nrecv =
            recv(ctx->conns->items[ctx->id]->sock, buf, buf_size, 0);
        if (nrecv > 0) {
            buf[nrecv] = '\0';
            printf("[%zx] recv: %s\n", pthread_self(), buf);
            channel_broadcast(ctx->conns->items[ctx->id]->sock, ctx->conns, buf,
                              ctx->conns->items[ctx->id]->channel_id);
        } else if (nrecv == 0) {
            printf("client disconnected\n");
            break;
        } else {
            perror("recv");
            break;
        }
    }

    return NULL;
}

int main() {
    Connections* conns = create_connections(1000);

    pthread_t workers[NWORKERS];
    worker_ctx_t worker_ctxs[NWORKERS];

    for (size_t i = 0; i < NWORKERS; ++i) {
        worker_ctxs[i] = (worker_ctx_t){.id = i, .conns = conns};
    }

    for (size_t i = 0; i < NWORKERS; ++i) {
        if (pthread_create(&workers[i], NULL, handle_connection,
                           &worker_ctxs[i]) == -1) {
            perror("failed to create worker thraed");
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
        perror("invalid ip address");
        exit(EXIT_FAILURE);
    }

    int ss = socket(AF_INET, SOCK_STREAM, 0);
    if (ss == -1) {
        perror("failed to create server socket");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        perror("failed to set socket option REUSEADDR");
        exit(EXIT_FAILURE);
    }

    if (bind(ss, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(ss, MAX_CONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        int cs = accept(ss, NULL, NULL);
        if (cs == -1) {
            perror("failed to create client socket");
            continue;
        }

        size_t buf_size = 1024;
        char buf[buf_size];
        ssize_t nrecv = recv(cs, buf, buf_size, 0);
        if (nrecv > 0) {
            buf[nrecv] = '\0';
            register_connection(cs, buf, conns);
        } else if (nrecv == 0) {
            printf("client disconnected\n");
        } else {
            perror("recv");
        }
    }

    for (size_t i = 0; i < NWORKERS; ++i) {
        if (pthread_join(workers[i], NULL) == -1) {
            perror("failed to join worker thread");
        }
    }

    connections_destroy(conns);
}