#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(
        std::size_t threadCount,
        std::size_t queueCapacity = 8);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Function>
    auto submit(Function&& function)
        -> std::future<std::invoke_result_t<std::decay_t<Function>&>> {
        using Result = std::invoke_result_t<std::decay_t<Function>&>;

        auto task = std::make_shared<std::packaged_task<Result()>>(
            std::forward<Function>(function));

        auto result = task->get_future();

        enqueue([task] {
            (*task)();
        });

        return result;
    }

private:
    void enqueue(std::function<void()> task);
    void workerLoop();
    void stopAndJoin() noexcept;

    std::mutex mutex_;
    std::condition_variable taskAvailable_;
    std::condition_variable spaceAvailable_;

    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;

    std::size_t queueCapacity_;
    bool stopping_ = false;
};