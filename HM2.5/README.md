# HM2.5：基于 pthread 的固定大小线程池

本作业使用 POSIX Threads（pthread）实现一个固定大小的线程池。线程池支持任务提交、并行执行、等待当前批次任务完成，以及安全地停止并回收所有工作线程。

实现位于 [`src.hpp`](./src.hpp)。

## 功能要求

需要补全以下接口：

```cpp
class ThreadPool {
public:
    using Task = void (*)(void *);

    explicit ThreadPool(std::size_t thread_count);
    void enqueue(Task function, void *argument);
    void wait();
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
};
```

- 构造时创建固定数量的工作线程；
- `enqueue` 将任务加入共享队列，任务按照队首顺序取出；
- 每个任务执行且只执行一次；
- `wait` 阻塞至队列中没有待执行任务，并且没有正在执行的任务；
- `wait` 返回后线程池仍然可用，可以继续提交下一批任务；
- 析构时通知工作线程退出，并使用 `pthread_join` 回收线程；
- 支持多个外部线程并发调用 `enqueue`；
- 支持多个外部线程并发调用 `wait`。

## 实现思路

线程池主要维护以下共享状态：

| 成员 | 作用 |
| --- | --- |
| `task_` | 保存待执行任务的 FIFO 队列 |
| `threads_` | 保存工作线程的 `pthread_t` |
| `mutex_` | 保护任务队列及其他共享状态 |
| `task_cond_` | 队列为空时阻塞工作线程，有新任务时进行通知 |
| `finished_cond_` | 阻塞 `wait`，全部任务完成时唤醒等待者 |
| `activeCount_` | 记录当前正在执行任务的工作线程数 |
| `stopping` | 标记线程池是否正在停止 |

队列中的每一项由函数指针和参数指针组成：

```cpp
struct TaskItem {
    Task function;
    void *argument;
};
```

工作线程的执行流程如下：

1. 获取互斥锁并检查任务队列；
2. 如果队列为空且线程池没有停止，则在 `task_cond_` 上阻塞；
3. 从队首取出一个任务，并增加 `activeCount_`；
4. 释放互斥锁，执行 `function(argument)`；
5. 任务完成后再次加锁并减少 `activeCount_`；
6. 当任务队列为空且 `activeCount_ == 0` 时，广播唤醒所有调用 `wait` 的线程。

任务函数在互斥锁之外执行，避免耗时任务长期占用锁，使多个工作线程能够真正并行处理任务。

## 条件变量与完成条件

条件变量可能出现虚假唤醒，因此工作线程和 `wait` 都使用 `while` 循环重新检查条件：

```cpp
while (task_.empty() && !stopping) {
    pthread_cond_wait(&task_cond_, &mutex_);
}
```

```cpp
while (!task_.empty() || activeCount_ != 0) {
    pthread_cond_wait(&finished_cond_, &mutex_);
}
```

仅判断任务队列为空并不足以说明任务已经全部完成，因为任务可能已经出队但仍在执行。因此，`wait` 的完成条件必须同时满足：

```text
任务队列为空 && 正在执行的任务数为 0
```

完成时使用 `pthread_cond_broadcast`，使并发调用 `wait` 的所有外部线程都能被唤醒。

## 析构过程

根据题目约定，析构前已经调用过 `wait`，此时不存在等待执行或正在执行的任务。析构函数执行以下操作：

1. 在互斥锁保护下将 `stopping` 设置为 `true`；
2. 广播 `task_cond_`，唤醒所有空闲的工作线程；
3. 工作线程观察到停止标志后退出循环；
4. 使用 `pthread_join` 回收全部线程；
5. 销毁条件变量和互斥锁。

## 编译与运行

需要在支持 pthread 的 Linux 环境中编译，并添加 `-pthread` 选项。进入本目录后，可以编译单个测试：

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pthread -I. 3192/1.cpp -o test1
./test1
```

也可以依次编译并运行全部测试：

```bash
for i in {1..7}; do
    g++ -std=c++17 -O2 -Wall -Wextra -pthread -I. "3192/$i.cpp" -o "test$i"
    "./test$i"
done
```


