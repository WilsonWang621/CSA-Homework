#include "src.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> task_started{false};
std::atomic<bool> release_task{false};

void gatedTask(void *) {
  task_started.store(true, std::memory_order_release);
  while (!release_task.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

} // namespace

int main() {
  constexpr int waiter_count = 8;
  std::atomic<int> entered{0};
  std::atomic<int> returned{0};

  ThreadPool pool(2);
  pool.enqueue(gatedTask, nullptr);

  while (!task_started.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  std::vector<std::thread> waiters;
  waiters.reserve(waiter_count);
  for (int i = 0; i < waiter_count; ++i) {
    waiters.emplace_back([&] {
      entered.fetch_add(1, std::memory_order_release);
      pool.wait();
      returned.fetch_add(1, std::memory_order_relaxed);
    });
  }

  while (entered.load(std::memory_order_acquire) != waiter_count) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  const bool all_blocked = returned.load(std::memory_order_relaxed) == 0;
  release_task.store(true, std::memory_order_release);

  for (auto &waiter : waiters) {
    waiter.join();
  }

  std::puts(all_blocked &&
                    returned.load(std::memory_order_relaxed) == waiter_count
                ? "passed"
                : "failed");
  return 0;
}

