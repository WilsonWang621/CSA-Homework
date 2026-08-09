#include "src.hpp"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

std::atomic<int> completed{0};

void increment(void *) {
  completed.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

int main() {
  constexpr int producer_count = 8;
  constexpr int tasks_per_producer = 5000;

  ThreadPool pool(4);
  std::vector<std::thread> producers;
  producers.reserve(producer_count);

  for (int producer = 0; producer < producer_count; ++producer) {
    producers.emplace_back([&] {
      for (int i = 0; i < tasks_per_producer; ++i) {
        pool.enqueue(increment, nullptr);
      }
    });
  }

  for (auto &producer : producers) {
    producer.join();
  }
  pool.wait();

  std::puts(completed.load(std::memory_order_relaxed) ==
                    producer_count * tasks_per_producer
                ? "passed"
                : "failed");
  return 0;
}

