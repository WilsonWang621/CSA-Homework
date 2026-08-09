#include "src.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {

std::atomic<int> completed{0};
std::atomic<bool> task_started{false};
std::atomic<bool> release_task{false};
std::atomic<bool> wait_returned{false};

void increment(void *) {
  completed.fetch_add(1, std::memory_order_relaxed);
}

void gatedTask(void *) {
  task_started.store(true, std::memory_order_release);
  while (!release_task.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  completed.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

int main() {
  bool ok = true;

  {
    ThreadPool pool(4);

    pool.wait();

    for (int i = 0; i < 2000; ++i) {
      pool.enqueue(increment, nullptr);
    }
    pool.wait();
    ok = ok && completed.load(std::memory_order_relaxed) == 2000;

    for (int i = 0; i < 2000; ++i) {
      pool.enqueue(increment, nullptr);
    }
    pool.wait();
    ok = ok && completed.load(std::memory_order_relaxed) == 4000;
  }

  {
    ThreadPool pool(1);
    pool.enqueue(gatedTask, nullptr);

    while (!task_started.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    std::thread waiter([&] {
      pool.wait();
      wait_returned.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    ok = ok && !wait_returned.load(std::memory_order_acquire);

    release_task.store(true, std::memory_order_release);
    waiter.join();
    ok = ok && wait_returned.load(std::memory_order_acquire);
    ok = ok && completed.load(std::memory_order_relaxed) == 4001;
  }

  std::puts(ok ? "passed" : "failed");
  return 0;
}

