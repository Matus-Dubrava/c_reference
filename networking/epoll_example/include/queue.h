#ifndef QUEUE_H
#define QUEUE_H

#include "stdlib.h"

typedef struct Node {
    int value;
    struct Node* next;
    struct Node* prev;
} Node;

typedef struct Queue {
    Node* head;
    Node* tail;
    size_t size;
} Queue;

Node* create_node(int value);
void queue_init(Queue* q);
int queue_add(Queue* q, int value);
int queue_pop(Queue* q);

#endif