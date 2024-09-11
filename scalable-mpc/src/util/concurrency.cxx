#include "util/concurrency.hpp"

void MULTI_TASK(std::function<void(size_t, size_t)> func, size_t num_tasks) {
  std::vector<std::thread> threads;

  // Start all threads based on the determined THREAD_COUNT
  for (size_t thread_id = 0; thread_id < THREAD_COUNT; thread_id++) {
    size_t start = thread_id * ((num_tasks + THREAD_COUNT - 1) / THREAD_COUNT);
    size_t end = std::min(start + ((num_tasks + THREAD_COUNT - 1) / THREAD_COUNT), num_tasks);
    threads.emplace_back(func, start, end);
  }

  // Wait for all threads to complete
  for (std::thread& thread : threads) {
    thread.join();
  }
}

