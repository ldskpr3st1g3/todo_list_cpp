#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class CpuThreadPool {
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::condition_variable cv_;
  std::mutex mtx_;
  bool stop_;

public:
  explicit CpuThreadPool(size_t threads);
  ~CpuThreadPool();
  auto enqueue(std::function<void()> task) -> void;
};
