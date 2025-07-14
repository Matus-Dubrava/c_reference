#include "stdio.h"
#include "stdlib.h"
#include "queue.h"
#include "assert.h"

void test_queue() {
    Queue q = {0};
    queue_init(&q);
    assert(q.size == 0);

    int values[] = {1, 2, 3, 4, 5};
    size_t len = sizeof(values) / sizeof(values[0]);
    for (size_t i = 0; i < len; ++i) {
        queue_add(&q, values[i]);
    }

    assert(q.size == len);

    int counter = 0;
    while (q.size != 0) {
        assert(queue_pop(&q) == values[counter++]);
    }

    assert(q.size == 0);
}

int main() {
    test_queue();
}