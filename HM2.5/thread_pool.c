#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* 线程池中任务函数的统一类型：接收一个 void * 参数，不返回结果。 */
typedef void (*ptr_to_func)(void *);
typedef void arg_to_func;

/*
 * 一个等待执行的任务。
 * func 和 args 对应课件中的函数指针与函数参数。
 */
typedef struct Task {
    ptr_to_func func;
    arg_to_func *args;
    struct Task *next;
} Task;

typedef struct Runtime Runtime;

/*
 * 每个 pthread 都收到一个独立的 Args。
 * done 是原子退出标志；runtime 指向线程共享的任务队列。
 */
typedef struct Args {
    Runtime *runtime;
    atomic_int done;
} Args;

struct Runtime {
    pthread_t *threads;
    Args *worker_args;
    size_t thread_count;

    Task *queue_head;
    Task *queue_tail;
    size_t unfinished_tasks;

    pthread_mutex_t mutex;
    pthread_cond_t task_available;
    pthread_cond_t all_tasks_done;

    int accepting_tasks;
};

/* 从队首取出一个任务。调用者必须已经持有 runtime->mutex。 */
static Task *pop_task(Runtime *runtime) {
    Task *task = runtime->queue_head;

    if (task != NULL) {
        runtime->queue_head = task->next;
        if (runtime->queue_head == NULL) {
            runtime->queue_tail = NULL;
        }
        task->next = NULL;
    }

    return task;
}

/* pthread 的线程入口函数。 */
static void *worker_main(void *raw_args) {
    Args *worker = (Args *)raw_args;
    Runtime *runtime = worker->runtime;

    for (;;) {
        Task *task;

        pthread_mutex_lock(&runtime->mutex);

        /* 没有任务时休眠；条件变量允许线程不占用 CPU。 */
        while (runtime->queue_head == NULL &&
               atomic_load_explicit(&worker->done, memory_order_acquire) == 0) {
            pthread_cond_wait(&runtime->task_available, &runtime->mutex);
        }

        /* 收到退出信号，并且队列已经清空时结束线程。 */
        if (runtime->queue_head == NULL &&
            atomic_load_explicit(&worker->done, memory_order_acquire) != 0) {
            pthread_mutex_unlock(&runtime->mutex);
            break;
        }

        task = pop_task(runtime);
        pthread_mutex_unlock(&runtime->mutex);

        /* 按课件要求：函数或参数为空时直接跳过。 */
        if (task != NULL && task->func != NULL && task->args != NULL) {
            task->func(task->args);
        }

        free(task);

        pthread_mutex_lock(&runtime->mutex);
        --runtime->unfinished_tasks;
        if (runtime->unfinished_tasks == 0) {
            pthread_cond_broadcast(&runtime->all_tasks_done);
        }
        pthread_mutex_unlock(&runtime->mutex);
    }

    return NULL;
}

/*
 * 初始化线程池。
 * 成功返回 0；失败返回一个 pthread 错误码或 ENOMEM/EINVAL。
 */
static int runtime_init(Runtime *runtime, size_t thread_count) {
    size_t created = 0;
    int rc;

    if (runtime == NULL || thread_count == 0) {
        return EINVAL;
    }

    runtime->threads = NULL;
    runtime->worker_args = NULL;
    runtime->thread_count = thread_count;
    runtime->queue_head = NULL;
    runtime->queue_tail = NULL;
    runtime->unfinished_tasks = 0;
    runtime->accepting_tasks = 1;

    runtime->threads = calloc(thread_count, sizeof(*runtime->threads));
    runtime->worker_args = calloc(thread_count, sizeof(*runtime->worker_args));
    if (runtime->threads == NULL || runtime->worker_args == NULL) {
        free(runtime->threads);
        free(runtime->worker_args);
        return ENOMEM;
    }

    rc = pthread_mutex_init(&runtime->mutex, NULL);
    if (rc != 0) {
        free(runtime->threads);
        free(runtime->worker_args);
        return rc;
    }

    rc = pthread_cond_init(&runtime->task_available, NULL);
    if (rc != 0) {
        pthread_mutex_destroy(&runtime->mutex);
        free(runtime->threads);
        free(runtime->worker_args);
        return rc;
    }

    rc = pthread_cond_init(&runtime->all_tasks_done, NULL);
    if (rc != 0) {
        pthread_cond_destroy(&runtime->task_available);
        pthread_mutex_destroy(&runtime->mutex);
        free(runtime->threads);
        free(runtime->worker_args);
        return rc;
    }

    for (created = 0; created < thread_count; ++created) {
        runtime->worker_args[created].runtime = runtime;
        atomic_init(&runtime->worker_args[created].done, 0);

        rc = pthread_create(&runtime->threads[created], NULL, worker_main,
                            &runtime->worker_args[created]);
        if (rc != 0) {
            size_t i;

            for (i = 0; i < created; ++i) {
                atomic_store_explicit(&runtime->worker_args[i].done, 1,
                                      memory_order_release);
            }

            pthread_mutex_lock(&runtime->mutex);
            pthread_cond_broadcast(&runtime->task_available);
            pthread_mutex_unlock(&runtime->mutex);

            for (i = 0; i < created; ++i) {
                pthread_join(runtime->threads[i], NULL);
            }

            pthread_cond_destroy(&runtime->all_tasks_done);
            pthread_cond_destroy(&runtime->task_available);
            pthread_mutex_destroy(&runtime->mutex);
            free(runtime->threads);
            free(runtime->worker_args);
            return rc;
        }
    }

    return 0;
}

/*
 * 向线程池提交任务。
 * 任务参数的内存至少要存活到该任务执行完成。
 */
static int runtime_submit(Runtime *runtime, ptr_to_func func, void *args) {
    Task *task;

    if (runtime == NULL) {
        return EINVAL;
    }

    task = malloc(sizeof(*task));
    if (task == NULL) {
        return ENOMEM;
    }

    task->func = func;
    task->args = args;
    task->next = NULL;

    pthread_mutex_lock(&runtime->mutex);

    if (!runtime->accepting_tasks) {
        pthread_mutex_unlock(&runtime->mutex);
        free(task);
        return ECANCELED;
    }

    if (runtime->queue_tail == NULL) {
        runtime->queue_head = task;
        runtime->queue_tail = task;
    } else {
        runtime->queue_tail->next = task;
        runtime->queue_tail = task;
    }

    ++runtime->unfinished_tasks;
    pthread_cond_signal(&runtime->task_available);
    pthread_mutex_unlock(&runtime->mutex);

    return 0;
}

/* 阻塞调用线程，直到已经提交的所有任务全部完成。 */
static void runtime_wait(Runtime *runtime) {
    if (runtime == NULL) {
        return;
    }

    pthread_mutex_lock(&runtime->mutex);
    while (runtime->unfinished_tasks != 0) {
        pthread_cond_wait(&runtime->all_tasks_done, &runtime->mutex);
    }
    pthread_mutex_unlock(&runtime->mutex);
}

/*
 * 停止接收新任务，等待当前任务完成，结束并回收所有线程。
 * runtime_destroy 返回后 runtime 不可再次使用，除非重新 runtime_init。
 */
static void runtime_destroy(Runtime *runtime) {
    size_t i;

    if (runtime == NULL || runtime->threads == NULL) {
        return;
    }

    pthread_mutex_lock(&runtime->mutex);
    runtime->accepting_tasks = 0;
    pthread_mutex_unlock(&runtime->mutex);

    runtime_wait(runtime);

    for (i = 0; i < runtime->thread_count; ++i) {
        atomic_store_explicit(&runtime->worker_args[i].done, 1,
                              memory_order_release);
    }

    pthread_mutex_lock(&runtime->mutex);
    pthread_cond_broadcast(&runtime->task_available);
    pthread_mutex_unlock(&runtime->mutex);

    for (i = 0; i < runtime->thread_count; ++i) {
        pthread_join(runtime->threads[i], NULL);
    }

    pthread_cond_destroy(&runtime->all_tasks_done);
    pthread_cond_destroy(&runtime->task_available);
    pthread_mutex_destroy(&runtime->mutex);

    free(runtime->threads);
    free(runtime->worker_args);

    runtime->threads = NULL;
    runtime->worker_args = NULL;
    runtime->thread_count = 0;
}

/* ------------------------- 以下是运行示例 ------------------------- */

typedef struct WorkItem {
    int input;
    int output;
} WorkItem;

static void calculate_square(void *raw_args) {
    WorkItem *item = (WorkItem *)raw_args;
    item->output = item->input * item->input;
}

int main(void) {
    enum { THREAD_COUNT = 4, TASK_COUNT = 16 };
    Runtime runtime;
    WorkItem items[TASK_COUNT];
    int rc;
    int i;

    rc = runtime_init(&runtime, THREAD_COUNT);
    if (rc != 0) {
        fprintf(stderr, "runtime_init failed: %d\n", rc);
        return EXIT_FAILURE;
    }

    for (i = 0; i < TASK_COUNT; ++i) {
        items[i].input = i + 1;
        items[i].output = 0;

        rc = runtime_submit(&runtime, calculate_square, &items[i]);
        if (rc != 0) {
            fprintf(stderr, "runtime_submit failed: %d\n", rc);
            runtime_destroy(&runtime);
            return EXIT_FAILURE;
        }
    }

    runtime_wait(&runtime);

    for (i = 0; i < TASK_COUNT; ++i) {
        printf("%2d * %2d = %3d\n", items[i].input, items[i].input,
               items[i].output);
    }

    runtime_destroy(&runtime);
    return EXIT_SUCCESS;
}