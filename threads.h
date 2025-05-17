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

// �򵥵��̳߳�ʾ��
// ע�⣺����һ���ǳ��������̳߳ء����������п�����Ҫ�����ӵ����ԣ�
// ���������ȼ�����̬�����߳����������õĴ����������ȡ�����ơ�

class ThreadPool {
public:
    /**
     * @brief ���캯��������ָ�������Ĺ����̡߳�
     * @param numThreads �̳߳��е��߳�������Ĭ��ΪӲ����������
     */
    ThreadPool(size_t numThreads = 0);

    /**
     * @brief �����������ȴ�����������ɲ������̡߳�
     */
    ~ThreadPool();

    // ��ֹ�����͸�ֵ
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief ���̳߳��ύһ������
     * @tparam F �������͡�
     * @tparam Args �������͡�
     * @param f Ҫִ�еĺ�����
     * @param args �����Ĳ�����
     * @return std::future<typename std::result_of<F(Args...)>::type> ���ڻ�ȡ��������
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        // For C++14 or earlier, use std::result_of<F(Args...)>::type
        // For C++17 and later, std::invoke_result is preferred.
        // VS2022 supports C++17 and later well.
        using return_type = typename std::invoke_result<F, Args...>::type;

        // ʹ�� std::packaged_task ����װ�����Ա��ȡ future
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            // ���������̳߳�ֹͣ�����������
            if (m_stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            m_tasks.emplace_back([task]() { (*task)(); }); // C2065 for m_tasks was due to deque not being declared.
        }
        m_condition.notify_one(); // ֪ͨһ���ȴ����߳�
        return res;
    }


    /**
     * @brief ��ȡ��ǰ�����е�����������
     * @return ����������
     */
    size_t GetTaskQueueSize() const;


private:
    void worker_thread(); // �����̵߳�ִ�к���

    std::vector<std::thread> m_workers;            // �洢�����̵߳�����
    std::deque<std::function<void()>> m_tasks;     // ������� (std::deque was missing include)

    std::mutex m_queueMutex;                       // ����������еĻ�����
    std::condition_variable m_condition;           // ��������������֪ͨ�����߳���������
    std::atomic<bool> m_stop;                      // ԭ�Ӳ���ֵ������ֹͣ�����߳�
};

#endif // THREADS_H
