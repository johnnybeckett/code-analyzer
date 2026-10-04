#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief A fixed-size, reusable thread pool.
 *
 * Producers `submit` tasks; a fixed number of worker threads pop and run them
 * in a FIFO order. Each task is handed off as a `std::packaged_task`, so the
 * caller receives a `std::future` for the task's return value *or* the
 * exception it threw. Because `packaged_task` catches and forwards that
 * exception, a worker never dies from a bad task: one throwing task is
 * isolated and delivered to its own future, and the pool keeps running.
 *
 * Not copyable (it owns live threads). Destroying the pool — or calling
 * `stop()` — shuts it down and joins every worker, so all in-flight tasks
 * complete before the destructor returns.
 *
 * This is the generic engine the analyzer's parsing reactor builds on
 * (producer/consumer); it knows nothing about files or classes.
 */
class ThreadPool {
public:
    /**
     * @brief Start `worker_count` workers.
     * @param worker_count Thread count; a value of 0 is treated as 1.
     */
    explicit ThreadPool(std::size_t worker_count);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Shut down the pool and join every worker.
     *
     * Idempotent. In-flight tasks run to completion; queued-but-unstarted
     * tasks are drained by the workers before they observe the stop flag.
     */
    ~ThreadPool();

    /**
     * @brief Enqueue a void task to run on a worker.
     * @return A `std::future<void>`; `future::get()` rethrows if the task threw
     *         (isolated — it never kills the worker or the pool).
     * @throws std::runtime_error if called after `stop()`/destruction began.
     */
    std::future<void> submit(std::function<void()> task) {
        auto wrapped = std::make_shared<std::packaged_task<void()>>(
            std::move(task));
        std::future<void> result = wrapped->get_future();
        {
            const std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stopped_) {
                throw std::runtime_error("ThreadPool::submit after stop()");
            }
            tasks_.emplace_back([wrapped]() { (*wrapped)(); });
        }
        condition_.notify_one();
        return result;
    }

    /**
     * @brief Request shutdown and block until every worker has joined.
     */
    void stop();

    /** @brief The number of worker threads this pool started. */
    std::size_t worker_count() const noexcept { return workers_.size(); }

private:
    void worker_thread();

    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stopped_ = false;
};

#endif  // THREAD_POOL_H
