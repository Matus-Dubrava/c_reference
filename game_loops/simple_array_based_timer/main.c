#define _POSIX_C_SOURCE 199309L
#define _GUN_SOURCE
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include <x86intrin.h>

typedef struct Event {
    void* args;
    void* (*cb)(void*);
    struct timespec wait_time_seconds;
    struct timespec remaining;
} Event;

Event* event_create(void* (*cb)(void*), void* args, size_t wait_time_seconds) {
    Event* e;
    e = malloc(sizeof(*e));
    if (!e) {
        perror("malloc event");
        return NULL;
    }

    struct timespec wait_time;
    wait_time.tv_sec = wait_time_seconds;
    wait_time.tv_nsec = 0;

    struct timespec remaining;
    remaining.tv_sec = wait_time_seconds;
    remaining.tv_nsec = 0;

    e->cb = cb;
    e->args = args;
    e->wait_time_seconds = wait_time;
    e->remaining = remaining;
    return e;
}

void event_free(Event* e) {
    free(e);
}

void* event_exec_cb(Event* e) {
    return e->cb(e->args);
}

typedef struct Arr {
    Event** events;
    size_t size;
    size_t cap;
} Arr;

Arr* arr_create(size_t cap) {
    Arr* arr;
    arr = malloc(sizeof(*arr));
    if (!arr) {
        perror("malloc array");
        return NULL;
    }

    Event** events;
    events = malloc(cap * sizeof(*events));
    if (!events) {
        perror("malloc events");
        free(arr);
        return NULL;
    }

    arr->size = 0;
    arr->cap = cap;
    arr->events = events;
    return arr;
}

void arr_free(Arr* arr) {
    for (size_t i = 0; i < arr->size; ++i) {
        event_free(arr->events[i]);
    }
    free(arr->events);
    free(arr);
}

int arr_add(Arr* arr,
            void* (*cb)(void*),
            void* args,
            size_t wait_time_seconds) {
    if (arr->size >= arr->cap) {
        size_t new_cap = arr->cap * 2;
        Event** tmp = realloc(arr->events, new_cap * sizeof(*arr->events));
        if (!tmp) {
            perror("arr realloc");
            arr_free(arr);
            return -1;
        }
        arr->cap = new_cap;
        arr->events = tmp;
    }

    Event* e = event_create(cb, args, wait_time_seconds);
    if (!e) {
        return -1;
    }

    arr->events[arr->size++] = e;
    return 0;
}

int arr_remove(Arr* arr, size_t idx) {
    if (idx >= arr->size) {
        fprintf(stderr, "index out of bounds\n");
        return -1;
    }

    Event* tmp = arr->events[idx];
    arr->size--;
    arr->events[idx] = arr->events[arr->size];
    free(tmp);
    return 0;
}

void* event_example(void* args) {
    int* id = (int*)args;
    printf("running event %d\n", *id);
    return NULL;
}

size_t timer_to_ms(const struct timespec* timer) {
    return timer->tv_sec * 1000 + timer->tv_nsec / 1000000;
}

void subtract_ms_from_timer(struct timespec* timer, size_t ms) {
    size_t remaining_ms = timer_to_ms(timer);

    if (ms > remaining_ms) {
        remaining_ms = 0;
    } else {
        remaining_ms -= ms;
    }

    size_t remaining_sec = remaining_ms / 1000;
    size_t remaining_nsec = (remaining_ms % 1000) * 1000000;
    timer->tv_sec = remaining_sec;
    timer->tv_nsec = remaining_nsec;
}

int main() {
    srand(time(NULL));

    Arr* arr = arr_create(10);
    if (!arr) {
        exit(EXIT_FAILURE);
    }

    // int values[] = {1, 2, 3, 4, 5, 6};
    int values[] = {2};
    size_t len = sizeof(values) / sizeof(values[0]);
    for (size_t i = 0; i < len; ++i) {
        arr_add(arr, event_example, (void*)&values[i], i % 3 + 1);
    }

    struct timespec loop_wait, start, end;
    loop_wait.tv_nsec = 0;
    loop_wait.tv_sec = 1;

    size_t nticks = 0;
    for (;;) {
        size_t start_cycles = __rdtsc();
        size_t random_delay_ms = (rand() % 30) + 1;  // in ms
        struct timespec delay;
        delay.tv_sec = 0;
        delay.tv_nsec = random_delay_ms * 1000000;

        clock_gettime(CLOCK_MONOTONIC, &start);

        nanosleep(&delay, NULL);

        clock_gettime(CLOCK_MONOTONIC, &end);
        float elapsed_ms = (end.tv_nsec - start.tv_nsec) / 1e6 +
                           (end.tv_sec - start.tv_sec) * 1000;

        if (nticks % 10 == 0) {
            printf("[%zu] loop tick elapsed: %.4fms\n", nticks, elapsed_ms);
        }
        nticks++;

        // printf("timers: ");
        for (size_t i = 0; i < arr->size; ++i) {
            if (arr->events[i]->remaining.tv_sec <= 0 &&
                arr->events[i]->remaining.tv_nsec <= 0) {
                arr->events[i]->cb(arr->events[i]->args);
                arr_remove(arr, i);
            } else {
                subtract_ms_from_timer(&arr->events[i]->remaining, elapsed_ms);
                // printf("(%d): %zu, ", i,
                //        timer_to_ms(&arr->events[i]->remaining));
            }
        }
        // printf("\n");
        size_t end_cycles = __rdtsc();
        if (nticks % 10 == 0) {
            printf("[%zu] cycles elapsed: %zu\n", nticks,
                   end_cycles - start_cycles);
        }
    }
}