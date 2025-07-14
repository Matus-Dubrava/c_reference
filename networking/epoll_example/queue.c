#include "stdio.h"
#include "stdlib.h"
#include "queue.h"

Node* create_node(int value) {
    Node* n = malloc(sizeof(Node));
    if (!n) {
        perror("malloc node");
        return NULL;
    }

    n->value = value;
    n->prev = NULL;
    n->next = NULL;
    return n;
}

void queue_init(Queue* q) {
    q->size = 0;
    q->head = NULL;
    q->tail = NULL;
}

int queue_add(Queue* q, int value) {
    Node* n = create_node(value);
    if (!n) {
        return -1;
    }

    if (q->size == 0) {
        q->head = n;
        q->tail = n;
    } else {
        n->next = q->tail;
        q->tail->prev = n;
        q->tail = n;
    }

    q->size++;
    return 0;
}

int queue_pop(Queue* q) {
    if (q->size == 0) {
        return -1;
    }

    Node* n = q->head;
    int value = n->value;
    q->head = q->head->prev;
    if (q->head) {
        q->head->next = NULL;
    } else {
        q->head = q->tail = NULL;
    }
    q->size--;
    free(n);
    return value;
}
