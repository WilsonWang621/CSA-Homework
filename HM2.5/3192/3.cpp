#include "src.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {

std::atomic<int> started{0};
std::atomic<bool> release_tasks{false};

void gatedTask(void *) {
  started.fetch_add(1, std::memory_order_relaxed);
  while (!release_tasks.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

} // namespace

int main() {
  ThreadPool pool(2);
  pool.enqueue(gatedTask, nullptr);
  pool.enqueue(gatedTask, nullptr);

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
  while (started.load(std::memory_order_relaxed) < 2 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }

  const bool ran_in_parallel = started.load(std::memory_order_relaxed) == 2;
  release_tasks.store(true, std::memory_order_release);
  pool.wait();

  std::puts(ran_in_parallel ? "passed" : "failed");
  return 0;
}

