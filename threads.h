#ifndef THREADS_H
#define THREADS_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <atomic>
#include <future>    // For std::async, std::future
#include <deque>     // For std::deque (was missing, caused C2039)
#include <type_traits> // For std::invoke_result (C++17) or std::result_of (C++11/14)

// 简单的线程池示例
// 注意：这是一个非常基础的线程池。生产环境中可能需要更复杂的特性，
// 如任务优先级、动态调整线程数量、更好的错误处理和任务取消机制。

class ThreadPool {
public:
    /**
     * @brief 构造函数，创建指定数量的工作线程。
     * @param numThreads 线程池中的线程数量。默认为硬件并发数。
     */
    ThreadPool(size_t numThreads = 0);

    /**
     * @brief 析构函数，等待所有任务完成并销毁线程。
     */
    ~ThreadPool();

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief 向线程池提交一个任务。
     * @tparam F 函数类型。
     * @tparam Args 参数类型。
     * @param f 要执行的函数。
     * @param args 函数的参数。
     * @return std::future<typename std::result_of<F(Args...)>::type> 用于获取任务结果。
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        // For C++14 or earlier, use std::result_of<F(Args...)>::type
        // For C++17 and later, std::invoke_result is preferred.
        // VS2022 supports C++17 and later well.
        using return_type = typename std::invoke_result<F, Args...>::type;

        // 使用 std::packaged_task 来包装任务，以便获取 future
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            // 不允许在线程池停止后添加新任务
            if (m_stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            m_tasks.emplace_back([task]() { (*task)(); }); // C2065 for m_tasks was due to deque not being declared.
        }
        m_condition.notify_one(); // 通知一个等待的线程
        return res;
    }


    /**
     * @brief 获取当前队列中的任务数量。
     * @return 任务数量。
     */
    size_t GetTaskQueueSize() const;


private:
    void worker_thread(); // 工作线程的执行函数

    std::vector<std::thread> m_workers;            // 存储工作线程的容器
    std::deque<std::function<void()>> m_tasks;     // 任务队列 (std::deque was missing include)

    std::mutex m_queueMutex;                       // 保护任务队列的互斥锁
    std::condition_variable m_condition;           // 条件变量，用于通知工作线程有新任务
    std::atomic<bool> m_stop;                      // 原子布尔值，用于停止所有线程
};

#endif // THREADS_H
