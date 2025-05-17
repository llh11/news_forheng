#include "log.h"
#include "globals.h" // 用于获取日志文件路径
#include <iostream>  // 用于在日志文件打开失败时输出到控制台
#include <time.h>    // For time_t, tm, localtime_s, wcsftime

// Logger class implementation
Logger::Logger() : m_logLevel(LogLevel::DEBUG) { // 默认日志级别
    // Constructor
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    }
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::SetLogLevel(LogLevel level) {
    m_logLevel = level;
}

bool Logger::SetLogFile(const std::wstring& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    // Open in append mode. Use std::ios_base::app for wofstream as well.
    m_logFile.open(filePath, std::ios_base::out | std::ios_base::app);
    if (!m_logFile.is_open()) {
        std::wcerr << L"Failed to open log file: " << filePath << std::endl;
        return false;
    }
    // Set locale for the file stream to handle Unicode correctly, especially if writing UTF-8
    // This might be needed if default locale causes issues with wofstream and Unicode.
    // try {
    //     m_logFile.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));
    // } catch (const std::exception& e) {
    //     std::wcerr << L"Warning: Could not imbue UTF-8 locale to log file: " << e.what() << std::endl;
    // }

    m_logFile << GetTimestamp() << L" [INFO] Log file opened. Logging level: " << LogLevelToString(m_logLevel) << std::endl;
    return true;
}

void Logger::LogInternal(LogLevel level, std::wstringstream& wss) {
    // This function assumes level check has been done by the caller (Log template function)
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_logFile.is_open()) {
        // Fallback: print to console if file not open
        std::wcerr << GetTimestamp() << L" [" << LogLevelToString(level) << L"] " << wss.str() << std::endl;
        return;
    }

    m_logFile << GetTimestamp() << L" [" << LogLevelToString(level) << L"] " << wss.str() << std::endl;
    m_logFile.flush();

    if (level == LogLevel::FATAL) {
        // For FATAL errors, consider more drastic actions if needed, e.g.,
        // m_logFile.close();
        // exit(EXIT_FAILURE); // Or std::terminate();
    }
}


std::wstring Logger::LogLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:   return L"DEBUG";
    case LogLevel::INFO:    return L"INFO";
    case LogLevel::WARNING: return L"WARNING";
    //case LogLevel::ERROR:   return L"ERROR";/*THis is wrong*/
    case LogLevel::FATAL:   return L"FATAL"; // C2838 was on the template declaration, not here.
    default:                return L"UNKNOWN";
    }
}

std::wstring Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    std::tm timeinfo_tm; // Use a local tm struct for localtime_s

    // Use localtime_s for safety (C4996)
    if (localtime_s(&timeinfo_tm, &now_c) != 0) {
        // Error handling for localtime_s
        return L"[Error getting time] ";
    }

    std::wstringstream wss;
    // std::put_time is fine with VS2022 and <iomanip>
    // The error C2589, C2062 was likely due to missing <iomanip> or incorrect usage context.
    // Ensure <iomanip> is included in log.h.
    wss << std::put_time(&timeinfo_tm, L"%Y-%m-%d %H:%M:%S");

    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    wss << L'.' << std::setfill(L'0') << std::setw(3) << ms.count();

    return wss.str();
}
