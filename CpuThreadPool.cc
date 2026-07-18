#include "CpuThreadPool.h"

CpuThreadPool::CpuThreadPool(size_t threads) : stop_(false) {
  for (size_t i{}; i < threads; ++i) {
    this->workers_.emplace_back([this]() {
      while (1) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(this->mtx_);
          this->cv_.wait(lock, [this]() { return this->stop_ || !this->tasks_.empty(); });
          if (this->stop_ && this->tasks_.empty())
            return;
          task = std::move(this->tasks_.front());
          this->tasks_.pop();
        }
        task();
      }
    });
  }
}

CpuThreadPool::~CpuThreadPool() {
  std::unique_lock<std::mutex> lock(this->mtx_);
  this->stop_ = true;
  lock.unlock();
  this->cv_.notify_all();
  for (std::thread& worker : this->workers_)
    if (worker.joinable())
      worker.join();
}

void CpuThreadPool::enqueue(std::function<void()> task) {
  std::unique_lock<std::mutex> lock(this->mtx_);
  this->tasks_.push(task);
  lock.unlock();
  this->cv_.notify_one();
}