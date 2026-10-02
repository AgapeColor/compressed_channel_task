#include "thread_pool.hpp"

#include <stdexcept>

ThreadPool::ThreadPool(
    std::size_t threadCount,
    std::size_t queueCapacity)
    : queueCapacity_(queueCapacity) {
    if (threadCount == 0 || queueCapacity == 0) {
        throw std::invalid_argument(
            "ThreadPool: thread count and queue capacity must be positive");
    }

    workers_.reserve(threadCount);

    try {
        for (std::size_t index = 0; index < threadCount; ++index) {
            workers_.emplace_back([this] {
                workerLoop();
            });
        }
    } catch (...) {
        stopAndJoin();
        throw;
    }
}

ThreadPool::~ThreadPool() {
    stopAndJoin();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);

        spaceAvailable_.wait(lock, [this] {
            return stopping_ || tasks_.size() < queueCapacity_;
        });

        if (stopping_) {
            throw std::runtime_error(
                "ThreadPool::enqueue(): pool is stopping");
        }

        tasks_.push(std::move(task));
    }

    taskAvailable_.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            taskAvailable_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });

            if (stopping_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        spaceAvailable_.notify_one();

        task();
    }
}

void ThreadPool::stopAndJoin() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    taskAvailable_.notify_all();
    spaceAvailable_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}