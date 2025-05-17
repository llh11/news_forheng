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

// ��־����
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    //ERROR,
    FATAL
};

class Logger {
public:
    // ��ȡ Logger ����ʵ��
    static Logger& GetInstance();

    // ��ֹ�����͸�ֵ
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief ������־����
     * @param level Ҫ���õ������־����
     */
    void SetLogLevel(LogLevel level);

    /**
     * @brief ������־�ļ�·��
     * @param filePath ��־�ļ�������·��
     * @return ����ɹ���/������־�ļ��򷵻� true�����򷵻� false
     */
    bool SetLogFile(const std::wstring& filePath);

    /**
     * @brief ��¼��־ (����ʵ��)
     * @param level ��־����
     * @param wss   �����Ѹ�ʽ����Ϣ�Ŀ��ַ�����
     */
    void LogInternal(LogLevel level, std::wstringstream& wss);

    /**
     * @brief ��¼��־ (ģ��汾���û�����)
     * @tparam Args ��������
     * @param level ��־����
     * @param message ��־��Ϣ����
     * @param args �ɱ����������ƴ�ӵ���Ϣ�����
     */
    template<typename... Args>
    void Log(LogLevel level, const std::wstring& message, Args... args) {
        if (level < m_logLevel) {
            return; // �����趨�������־����¼
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
    // ˽�й��캯����ȷ������ģʽ
    Logger();
    // ˽����������
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


    std::wofstream m_logFile;        // ��־�ļ��� (ʹ�� wofstream ֧�� Unicode)
    LogLevel m_logLevel;           // ��ǰ��־����
    std::mutex m_mutex;            // ���ڱ������ļ�д��Ļ�����
};

// �궨�����־����
#define LOG_DEBUG(...) Logger::GetInstance().Debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::GetInstance().Info(__VA_ARGS__)
#define LOG_WARNING(...) Logger::GetInstance().Warning(__VA_ARGS__)
#define LOG_ERROR(...) Logger::GetInstance().Error(__VA_ARGS__)
#define LOG_FATAL(...) Logger::GetInstance().Fatal(__VA_ARGS__)

#endif // LOG_H
