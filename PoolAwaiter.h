#pragma once
#include "CpuThreadPool.h"
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThreadPool.h>

template <typename T> struct TaskState {
  std::optional<T> result;
  std::exception_ptr eptr;
  std::coroutine_handle<> handle;
  trantor::EventLoop* loop;
};

template <typename T, typename Func> class PoolAwaiter {
  Func func_;
  CpuThreadPool& pool_;
  std::shared_ptr<TaskState<T>> state_;

public:
  PoolAwaiter(Func func, CpuThreadPool& pool, std::shared_ptr<TaskState<T>> state)
      : func_(std::move(func)), pool_(pool), state_(std::move(state)) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) {
    this->state_->handle = h;
    auto state_copy = this->state_;
    this->pool_.enqueue([state_copy, f = std::move(this->func_)]() mutable {
      try {
        if constexpr (std::is_void_v<T>)
          f();
        else
          state_copy->result.emplace(f());
      } catch (...) {
        state_copy->eptr = std::current_exception();
      }
      state_copy->loop->queueInLoop([state_copy]() {
        if (state_copy->handle)
          state_copy->handle.resume();
      });
    });
  }

  T await_resume() {
    if (this->state_->eptr)
      std::rethrow_exception(this->state_->eptr);
    if constexpr (!std::is_void_v<T>) {
      return std::move(*this->state_->result);
    }
  }
};

template <typename Func>
auto run_in_pool(CpuThreadPool& pool, trantor::EventLoop* loop, Func&& func) {
  using ReturnType = std::invoke_result_t<Func>;
  auto state = std::make_shared<TaskState<ReturnType>>();
  state->loop = loop;
  return PoolAwaiter<ReturnType, Func>{std::forward<Func>(func), pool, state};
}
