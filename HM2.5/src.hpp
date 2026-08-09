#pragma once

#include <cstddef>
#include <pthread.h>
#include <vector>
#include <queue>


class ThreadPool {
public:
    using Task = void (*)(void *);

    struct TaskItem{
    /* data */
        Task function;
        void *argument;
    };

    explicit ThreadPool(std::size_t numThreads) : threads_(numThreads){
        // Your code here
        pthread_mutex_init(&mutex_, nullptr);
        pthread_cond_init(&task_cond_, nullptr);
        pthread_cond_init(&finished_cond_, nullptr);
        
        for(int i = 0; i < numThreads; i++){
            pthread_create(&threads_[i], nullptr, &work, this);
        }
    }

    void enqueue(Task task, void *arg) {
        // Your code here
        pthread_mutex_lock(&mutex_);
        task_.push(TaskItem{task, arg});
        pthread_mutex_unlock(&mutex_);

        pthread_cond_signal(&task_cond_);
    }

    void wait() {
        // Your code here
        pthread_mutex_lock(&mutex_);
        while(!task_.empty() || activeCount_ != 0){
            pthread_cond_wait(&finished_cond_, &mutex_);
        }
        pthread_mutex_unlock(&mutex_);

    }

    ~ThreadPool() {
        // Your code here
        pthread_mutex_lock(&mutex_);
        stopping = true;
        pthread_cond_broadcast(&task_cond_);
        pthread_mutex_unlock(&mutex_);
        for(int i = 0; i < threads_.size(); i++){
            pthread_join(threads_[i], nullptr);
        }
        pthread_cond_destroy(&task_cond_);
        pthread_cond_destroy(&finished_cond_);
        pthread_mutex_destroy(&mutex_);
    }

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // You can add private members and methods here as needed
    std::queue<TaskItem> task_;
    std::vector<pthread_t> threads_;

    pthread_mutex_t mutex_;

    pthread_cond_t task_cond_;
    pthread_cond_t finished_cond_;

    std::size_t activeCount_ = 0;

    bool stopping = false;

    static void* work(void* arg){
        ThreadPool* pool = static_cast<ThreadPool*>(arg);
        while(true){
            pthread_mutex_lock(&pool->mutex_);

            while(pool->task_.empty() && !pool->stopping){
                pthread_cond_wait(&pool->task_cond_, &pool->mutex_);
            }

            if(pool->stopping && pool->task_.empty()){
                pthread_mutex_unlock(&pool->mutex_);
                break;
            }

            pool->activeCount_++;
            TaskItem task = pool->task_.front();
            pool->task_.pop();

            pthread_mutex_unlock(&pool->mutex_);
            task.function(task.argument);

            pthread_mutex_lock(&pool->mutex_);
            pool->activeCount_--;

            if(pool->activeCount_ == 0 && pool->task_.empty()){
                pthread_cond_broadcast(&pool->finished_cond_);     
            }

            pthread_mutex_unlock(&pool->mutex_);
        }
        return nullptr;
    }
};