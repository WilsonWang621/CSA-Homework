#include "src.hpp"

#include <atomic>
#include <cstddef>
#include <cstdio>

namespace {

std::atomic<int> completed{0};

void increment(void *) {
  completed.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

int main() {
  constexpr std::size_t task_count = 50000;
  completed.store(0, std::memory_order_relaxed);

  ThreadPool pool(4);
  for (std::size_t i = 0; i < task_count; ++i) {
    pool.enqueue(increment, nullptr);
  }
  pool.wait();

  std::puts(completed.load(std::memory_order_relaxed) ==
                    static_cast<int>(task_count)
                ? "passed"
                : "failed");
  return 0;
}

