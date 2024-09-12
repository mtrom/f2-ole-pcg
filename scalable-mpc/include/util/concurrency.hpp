#pragma once

#include <algorithm>
#include <functional>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

// default thread count if hardware value is undefined
#define DEFAULT_THREAD_COUNT 8

// global constant for thread count, initialized in cxx
const size_t THREAD_COUNT = []() {
  size_t count = (
    std::thread::hardware_concurrency() > 0
    ? std::thread::hardware_concurrency()
    : DEFAULT_THREAD_COUNT
  );
  count /= 2;
  std::cout << "[Info] using thread count " << count << std::endl;
  return count;
}();

// spin up `THREAD_COUNT` treads which call `task` with the thread id
void MULTI_TASK(std::function<void(size_t, size_t)> task, size_t num_tasks);

// partition an iteration task across multiple threads and combine the results
template <typename T>
T TASK_REDUCE(
  std::function<T(size_t, size_t)> task,
  std::function<T(std::vector<T>)> combine,
  size_t num_tasks
) {
  // for tests where task sizes are small
  if (num_tasks < 8 * THREAD_COUNT) { return combine(std::vector<T>{task(0, num_tasks)}); }

  std::vector<std::future<T>> futures(THREAD_COUNT);
  std::vector<T> results(THREAD_COUNT);

  for (size_t thread_id = 0; thread_id < THREAD_COUNT; thread_id++) {
    size_t start = thread_id * ((num_tasks + THREAD_COUNT - 1) / THREAD_COUNT);
    size_t end = std::min(start + ((num_tasks + THREAD_COUNT - 1) / THREAD_COUNT), num_tasks);
    futures[thread_id] = std::async(std::launch::async, task, start, end);
  }

  for (size_t i = 0; i < THREAD_COUNT; i++) {
    results[i] = futures[i].get();
  }

  return combine(results);
}
