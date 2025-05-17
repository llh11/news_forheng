#ifndef LOG_H
#define LOG_H

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>     // For std::mutex
#include <chrono>    // For std::chrono
#include <iomanip>   // For std::put_time, std::setfill, std::setw
#include <vector>    // Required by some compilers for sstream parameter packing, or for general utility
#include <ios>       // For std::ios::out, std::ios::app

// Forward declaration if needed, or ensure it's included before
// class Logger; // Not strictly needed if all definitions are within this file or .cpp

// 日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    //ERROR,
    FATAL
};

class Logger {
public:
    // 获取 Logger 单例实例
    static Logger& GetInstance();

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 设置日志级别
     * @param level 要设置的最低日志级别
     */
    void SetLogLevel(LogLevel level);

    /**
     * @brief 设置日志文件路径
     * @param filePath 日志文件的完整路径
     * @return 如果成功打开/创建日志文件则返回 true，否则返回 false
     */
    bool SetLogFile(const std::wstring& filePath);

    /**
     * @brief 记录日志 (核心实现)
     * @param level 日志级别
     * @param wss   包含已格式化消息的宽字符串流
     */
    void LogInternal(LogLevel level, std::wstringstream& wss);

    /**
     * @brief 记录日志 (模板版本，用户调用)
     * @tparam Args 参数类型
     * @param level 日志级别
     * @param message 日志消息主体
     * @param args 可变参数，将被拼接到消息主体后
     */
    template<typename... Args>
    void Log(LogLevel level, const std::wstring& message, Args... args) {
        if (level < m_logLevel) {
            return; // 低于设定级别的日志不记录
        }

        std::wstringstream wss;
        wss << message;

        // C++17 fold expression for cleaner variadic template argument processing
        // ((wss << L" " << args), ...); // This can be problematic if args are not directly streamable or need formatting

        // Safer way to append arguments, especially if they are of mixed types
        // This requires C++17. If using an older standard, you'd need a recursive template or initializer list trick.
        // For VS2022, C++17 should be fine.
        // The issue C2760, C3878 often points to fold expressions not being parsed as expected or compiler not in C++17 mode.
        // Let's try a helper for argument packing if fold expressions are problematic.
        FormatArgs(wss, args...);


        LogInternal(level, wss); // Call the internal logging function
    }


    template<typename... Args>
    void Debug(const std::wstring& message, Args... args) {
        Log(LogLevel::DEBUG, message, args...);
    }

    template<typename... Args>
    void Info(const std::wstring& message, Args... args) {
        Log(LogLevel::INFO, message, args...);
    }

    template<typename... Args>
    void Warning(const std::wstring& message, Args... args) {
        Log(LogLevel::WARNING, message, args...);
    }

    template<typename... Args>
    void Error(const std::wstring& message, Args... args) {
       // Log(LogLevel::ERROR, message, args...);
    }

    template<typename... Args>
    void Fatal(const std::wstring& message, Args... args) { // C2838 was here. Ensure it's not LogLevel::FATAL
        Log(LogLevel::FATAL, message, args...);
    }


private:
    // 私有构造函数，确保单例模式
    Logger();
    // 私有析构函数
    ~Logger();

    std::wstring LogLevelToString(LogLevel level);
    std::wstring GetTimestamp();

    // Helper for variadic template argument formatting
    // Base case for recursion (no arguments left)
    void FormatArgs(std::wstringstream& wss) {}

    // Recursive step
    template<typename T, typename... Rest>
    void FormatArgs(std::wstringstream& wss, T&& first, Rest&&... rest) {
        wss << L" " << first;
        FormatArgs(wss, std::forward<Rest>(rest)...);
    }


    std::wofstream m_logFile;        // 日志文件流 (使用 wofstream 支持 Unicode)
    LogLevel m_logLevel;           // 当前日志级别
    std::mutex m_mutex;            // 用于保护对文件写入的互斥锁
};

// 宏定义简化日志调用
#define LOG_DEBUG(...) Logger::GetInstance().Debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::GetInstance().Info(__VA_ARGS__)
#define LOG_WARNING(...) Logger::GetInstance().Warning(__VA_ARGS__)
#define LOG_ERROR(...) Logger::GetInstance().Error(__VA_ARGS__)
#define LOG_FATAL(...) Logger::GetInstance().Fatal(__VA_ARGS__)

#endif // LOG_H
