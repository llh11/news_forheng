#include "threads.h"
#include "log.h"
// <deque> is now included in threads.h

ThreadPool::ThreadPool(size_t numThreads) : m_stop(false) {
    size_t hardwareThreads = std::thread::hardware_concurrency();
    size_t threadsToCreate = (numThreads == 0) ? (hardwareThreads == 0 ? 2 : hardwareThreads) : numThreads;
    // 如果无法检测硬件线程数，默认创建2个

    LOG_INFO(L"Initializing ThreadPool with ", threadsToCreate, L" threads.");

    m_workers.reserve(threadsToCreate);
    for (size_t i = 0; i < threadsToCreate; ++i) {
        m_workers.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool() {
    LOG_INFO(L"Shutting down ThreadPool...");
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true; // 设置停止标志
    }
    m_condition.notify_all(); // 唤醒所有等待的线程

    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join(); // 等待每个工作线程结束
        }
    }
    LOG_INFO(L"ThreadPool shut down complete.");
}

void ThreadPool::worker_thread() {
    LOG_DEBUG(L"Worker thread started. ID: ", std::this_thread::get_id());
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            // 等待条件：任务队列不为空，或者线程池已停止
            m_condition.wait(lock, [this] { return m_stop.load() || !m_tasks.empty(); }); // Use m_stop.load() for atomic

            // 如果线程池已停止且任务队列为空，则退出线程
            if (m_stop.load() && m_tasks.empty()) { // Use m_stop.load()
                LOG_DEBUG(L"Worker thread stopping. ID: ", std::this_thread::get_id());
                return;
            }

            // 获取任务
            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front()); // C2672 std::move should be fine with std::function
                m_tasks.pop_front();
            }
            else {
                continue;
            }
        }

        if (task) {
            try {
                LOG_DEBUG(L"Worker thread ", std::this_thread::get_id(), L" executing task.");
                task();
                LOG_DEBUG(L"Worker thread ", std::this_thread::get_id(), L" finished task.");
            }
            catch (const std::exception& e) {
                LOG_ERROR(L"Exception caught in worker thread while executing task: ", WideToUtf8(std::wstring(L"") + e.what()).c_str());
            }
            catch (...) {
                LOG_ERROR(L"Unknown exception caught in worker thread while executing task.");
            }
        }
    }
}

size_t ThreadPool::GetTaskQueueSize() const {
    // C2665 for unique_lock: const member functions need a mutable mutex or careful handling.
    // However, m_queueMutex is not declared mutable.
    // For a const method that locks, the mutex itself should be locked in a way that doesn't modify the logical state
    // of the ThreadPool from the caller's perspective. Accessing m_tasks.size() is a read operation.
    std::unique_lock<std::mutex> lock(m_queueMutex); // This should be fine. The error might have been a cascade.
    return m_tasks.size();
}
