#include "core/thread_pool.h"

ThreadPool::ThreadPool(std::size_t worker_count) {
    if (worker_count == 0) {
        worker_count = 1;
    }
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this] { worker_thread(); });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::stop() {
    {
        const std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stopped_) {
            return;  // already joined; idempotent
        }
        stopped_ = true;
    }
    condition_.notify_all();
    for (std::thread& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
}

void ThreadPool::worker_thread() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Block until there is work or the pool is shutting down.
            condition_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
            if (stopped_ && tasks_.empty()) {
                return;  // clean shutdown, queue drained
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        // Run unlocked so one slow task never blocks the rest. Any exception
        // is captured by the packaged_task and delivered to the caller's
        // future — it does not terminate this worker.
        task();
    }
}
