#include "src.hpp"

#include <atomic>
#include <cstdio>

namespace {

std::atomic<int> completed{0};

void increment(void *) {
  completed.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

int main() {
  constexpr int rounds = 100;
  constexpr int tasks_per_round = 50;

  for (int round = 0; round < rounds; ++round) {
    ThreadPool pool(2);
    for (int i = 0; i < tasks_per_round; ++i) {
      pool.enqueue(increment, nullptr);
    }
    pool.wait();
  }

  std::puts(completed.load(std::memory_order_relaxed) ==
                    rounds * tasks_per_round
                ? "passed"
                : "failed");
  return 0;
}

