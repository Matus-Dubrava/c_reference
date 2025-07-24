#define _GNU_SOURCE

#include "ctype.h"
#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "sys/epoll.h"
#include "fcntl.h"
#include "unistd.h"
#include "errno.h"

#define IP "127.0.0.1"
#define PORT 10000

#define START_CLIENTS 100

#define MAX_CONN 1000
#define MAX_CLIENTS 1000000
#define MAX_EVENTS 1000
#define MAX_NAME_LEN 100

#define MSG_EXIT "exit"

typedef struct Client {
    int sock;
    char* name;
} Client;

typedef struct Clients {
    Client** items;
    size_t len;
    size_t cap;
} Clients;

Client* client_create(int sock, char* name) {
    Client* c = malloc(sizeof(Client));
    if (!c) {
        perror("malloc client");
        return NULL;
    }

    c->sock = sock;
    c->name = strndup(name, MAX_NAME_LEN);
    return c;
}

void client_destroy(Client* c) {
    free(c->name);
    free(c);
}

bool client_update_name(Client* c, char* name, bool overwrite) {
    if (!overwrite && strncmp(c->name, "", 1) != 0) {
        return false;
    }

    free(c->name);
    c->name = strndup(name, MAX_NAME_LEN);
    return true;
}

Clients* clients_create(size_t cap) {
    Clients* clients = malloc(sizeof(Clients));
    if (!clients) {
        perror("malloc clients");
        return NULL;
    }

    Client** items = malloc(cap * sizeof(Client*));
    if (!items) {
        perror("malloc clients items");
        free(clients);
        return NULL;
    }

    clients->items = items;
    clients->len = 0;
    clients->cap = cap;
    return clients;
}

void clients_destroy(Clients* clients) {
    for (size_t i = 0; i < clients->len; ++i) {
        client_destroy(clients->items[i]);
    }
    free(clients->items);
    free(clients);
}

int clients_add(Clients* clients, int sock, char* name) {
    if (clients->len >= clients->cap && clients->cap >= MAX_CLIENTS) {
        fprintf(stderr, "max client capacity reached\n");
        return -1;
    }

    if (clients->len >= clients->cap && clients->cap < MAX_CLIENTS) {
        size_t new_cap = clients->cap * 2;
        Client** new_items = realloc(clients->items, new_cap * sizeof(Client*));
        if (!new_items) {
            perror("realloc clients items");
            // this seems a bit drastric; we should probably avoid wiping all
            // clients like this
            clients_destroy(clients);
            return -1;
        }

        clients->items = new_items;
        clients->cap = new_cap;
    }

    Client* c = client_create(sock, name);
    if (!c) {
        return -1;
    }

    clients->items[clients->len++] = c;
    return 0;
}

Client* clients_get_by_socket(Clients* clients, int sock) {
    for (size_t i = 0; i < clients->len; ++i) {
        if (clients->items[i]->sock == sock) {
            return clients->items[i];
        }
    }

    return NULL;
}

void client_disconnect(Client* client, Clients* clients) {
    bool move = false;

    for (size_t i = 0; i < clients->len; ++i) {
        if (clients->items[i]->sock == client->sock) {
            move = true;
            client_destroy(client);
        }

        if (move && i < clients->len) {
            clients->items[i] = clients->items[i + 1];
        }
    }

    if (move) {
        clients->len--;
    }
}

int setnonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;
}

void etrim_whitespaces(char* str) {
    char* end = str + strlen(str) - 1;

    while (end > str &&
           (isspace((unsigned char)*end) || (unsigned char)*end < 32 ||
            (unsigned char)*end > 126)) {
        end--;
    }

    *(end + 1) = '\0';
}

void broadcast(char* msg, Client* client, Clients* clients) {
    size_t len = strlen(msg) + 4 + strlen(client->name);
    char buf[len];
    memset(&buf, 0, sizeof(buf));
    sprintf(buf, "[%s] %s\n", client->name, msg);

    for (size_t i = 0; i < clients->len; ++i) {
        if (clients->items[i]->sock != client->sock) {
            ssize_t nsent = send(clients->items[i]->sock, buf, len, 0);
            if (nsent < 0) {
                perror("send");
            }
        }
    }
}

void process_message(char* msg, Client* client, Clients* clients) {
    printf("[%s]: %s\n", client->name, msg);
    broadcast(msg, client, clients);
}

void handle_request(int sock, Clients* clients) {
    size_t buf_size = 1024;
    char buf[buf_size];
    memset(&buf, 0, buf_size);

    for (;;) {
        ssize_t nrecv = recv(sock, buf, buf_size - 1, 0);
        if (nrecv > 0) {
            etrim_whitespaces(buf);

            // this lookup should be faster than linear array search since we
            // are doing it on every request
            Client* client = clients_get_by_socket(clients, sock);
            if (client && (strncmp(client->name, "", 1) == 0)) {
                bool overwrite_name = false;
                if (client_update_name(client, buf, overwrite_name)) {
                    printf("updated client %d name to %s\n", sock,
                           client->name);
                }
            } else {
                process_message(buf, client, clients);
            }
        } else if (nrecv == 0) {
            printf("client disconnected\n");
            Client* client = clients_get_by_socket(clients, sock);
            if (client) {
                client_disconnect(client, clients);
            }
            close(sock);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("recv");
            close(sock);
            break;
        }
    }
}

int main() {
    Clients* clients = clients_create(START_CLIENTS);
    if (!clients) {
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_EVENTS];
    int nfds, epollfd;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, IP, &addr.sin_addr) <= 0) {
        perror("invalid ip address");
        exit(EXIT_FAILURE);
    }

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == -1) {
        perror("failed to create server socket");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        perror("failed to set socket option REUSEADDR");
        exit(EXIT_FAILURE);
    }

    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(ls, MAX_CONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = ls;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ls, &ev) == -1) {
        perror("epoll_ctl: listen socket");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll wait");
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == ls) {
                int cs = accept(ls, NULL, NULL);
                if (cs == -1) {
                    perror("failed to create client socket");
                    exit(EXIT_FAILURE);
                }

                setnonblocking(cs);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = cs;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cs, &ev) == -1) {
                    perror("epoll_ctl: conn socket");
                    continue;
                }
                clients_add(clients, cs, "");
            } else {
                handle_request(events[i].data.fd, clients);
            }
        }
    }
}