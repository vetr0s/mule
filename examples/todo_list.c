/* arr: a growable array of structs, with insert, remove and pop. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct Task {
    const char *what;
    int priority;
} Task;
typedef Arr(Task) TaskList;

static void print(const TaskList *tasks) {
    for (size_t i = 0; i < tasks->len; i++)
        printf("  %d %s\n", tasks->data[i].priority, tasks->data[i].what);
}

int main(void) {
    TaskList tasks = { 0 };
    bool ok = arr_push(&tasks, ((Task){ "write the tests", 2 })) &&
              arr_push(&tasks, ((Task){ "ship it", 3 })) &&
              arr_insert(&tasks, 0, ((Task){ "read the spec", 1 }));
    if (!ok) {
        arr_free(&tasks);
        return EXIT_FAILURE;
    }
    puts("planned:");
    print(&tasks);

    arr_remove(&tasks, 1);
    Task last = arr_pop(&tasks);
    printf("dropped the tests, then popped \"%s\"\nleft:\n", last.what);
    print(&tasks);
    arr_free(&tasks);
    return EXIT_SUCCESS;
}
