#include "src.hpp"

#include <atomic>
#include <cstddef>
#include <cstdio>

namespace {

struct Argument {
  std::atomic<int> *seen;
};

void record(void *raw) {
  auto *argument = static_cast<Argument *>(raw);
  argument->seen->fetch_add(1, std::memory_order_relaxed);
}

} // namespace

int main() {
  constexpr std::size_t task_count = 128;
  std::atomic<int> seen[task_count]{};
  Argument arguments[task_count];

  ThreadPool pool(1);
  for (std::size_t i = 0; i < task_count; ++i) {
    arguments[i] = Argument{&seen[i]};
    pool.enqueue(record, &arguments[i]);
  }
  pool.wait();

  for (const auto &count : seen) {
    if (count.load(std::memory_order_relaxed) != 1) {
      std::puts("failed");
      return 0;
    }
  }

  std::puts("passed");
  return 0;
}

