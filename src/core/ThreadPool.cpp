#include "core/ThreadPool.hpp"

ThreadPool::ThreadPool(std::size_t n_threads)
{
    for (std::size_t i = 0; i < n_threads; ++i)
        workers_.emplace_back([this] { workerLoop(); });
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lk(queue_mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_)
        if (w.joinable()) w.join();
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(queue_mtx_);
            cv_.wait(lk, [this]{ return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}
