// log.cpp
// Implements logging functions.
#include "log.h"
#include "globals.h" // Access to g_logMutex, g_logCacheMutex, g_logCache, etc.
#include "utils.h"   // For utf8_to_wstring, wstring_to_utf8, EnsureDirectoryExists

#include <fstream>
#include <ctime>
#include <sstream>
#include <iostream> // For debugging output if needed
#include <iomanip>  // For std::put_time (alternative time formatting)
#include <deque>
#include <system_error> // For std::error_code, std::system_category
#include <chrono>       // For std::chrono::system_clock

// Logs a UTF-8 message with a timestamp to file and memory cache.
void Log(const std::string& message_utf8) {
    // 1. Ensure the log directory exists before attempting to write
    //    Note: EnsureDirectoryExists already logs errors internally via OutputDebugStringW
    if (!EnsureDirectoryExists(CONFIG_DIR)) {
        // Critical failure: Cannot log to file if directory doesn't exist. Log to debugger.
        // Construct the error message carefully
        std::string errorMsg = "严重错误：无法创建/访问日志目录 D:\\news\\ - 无法记录消息: "; // "CRITICAL ERROR: Cannot create/access log directory... - Cannot log message: "
        errorMsg += message_utf8;
        errorMsg += "\n";
        OutputDebugStringA(errorMsg.c_str());
        return;
    }

    // 2. Get current time and format it
    std::string timestamp_str;
    try {
        // Using chrono for potentially higher precision and modern C++ style
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_s(&now_tm, &now_c); // Use safer localtime_s

        // Using stringstream for formatting to avoid fixed buffer potential issues
        std::stringstream ss;
        ss << std::put_time(&now_tm, "[%Y-%m-%d %H:%M:%S]");
        timestamp_str = ss.str();
    }
    catch (const std::exception& e) {
        // Handle potential time formatting errors
        timestamp_str = "[time_error]";
        // Log error to debugger to avoid recursion if Log() is called here
        OutputDebugStringA(("获取或格式化时间戳时出错: " + std::string(e.what()) + "\n").c_str()); // "Error getting or formatting timestamp: "
    }
    catch (...) {
        timestamp_str = "[time_error]";
        OutputDebugStringA("获取或格式化时间戳时发生未知错误。\n"); // "Unknown error getting or formatting timestamp."
    }


    // 3. Add to in-memory log cache (thread-safe)
    try {
        std::wstring wide_log_entry = utf8_to_wstring(timestamp_str + " " + message_utf8);
        if (wide_log_entry.empty() && !(timestamp_str.empty() && message_utf8.empty())) {
            // Log conversion error if the result is empty but input wasn't
            OutputDebugStringA("警告：日志条目 UTF-8 -> wstring 转换失败。\n"); // "Warning: Log entry UTF-8 -> wstring conversion failed."
            // Optionally, add the original UTF-8 string to the cache if conversion fails?
            // For now, we just log the warning.
        }

        std::lock_guard<std::mutex> cache_lock(g_logCacheMutex); // Lock the cache mutex
        g_logCache.push_back(std::move(wide_log_entry)); // Move the string for efficiency
        // Limit cache size
        while (g_logCache.size() > MAX_LOG_LINES_DISPLAY) {
            g_logCache.pop_front();
        }
    }
    catch (const std::bad_alloc& ba) {
        OutputDebugStringA(("内存分配错误，无法将日志添加到缓存: " + std::string(ba.what()) + "\n").c_str()); // "Memory allocation error, cannot add log to cache: "
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("将日志添加到缓存时出错: " + std::string(e.what()) + "\n").c_str()); // "Error adding log to cache: "
    }
    catch (...) {
        OutputDebugStringA("将日志添加到缓存时发生未知错误。\n"); // "Unknown error adding log to cache."
    }
    // Mutex released automatically by lock_guard destructor

    // 4. Append to log file (thread-safe)
    { // Scope for file lock and handle
        std::lock_guard<std::mutex> file_lock(g_logMutex); // Lock the file mutex

        // Open file using RAII handle (FileHandle)
        // Use GENERIC_WRITE instead of FILE_APPEND_DATA for potentially more robust behavior across systems/versions,
        // although FILE_APPEND_DATA is usually correct. Combine with SetFilePointer to ensure appending.
        FileHandle hFile(CreateFileW(
            LOG_PATH.c_str(),
            GENERIC_WRITE,         // Request write access
            FILE_SHARE_READ,       // Allow reading while open
            nullptr,               // Default security
            OPEN_ALWAYS,           // Create if not exists, open if exists
            FILE_ATTRIBUTE_NORMAL, // Normal file attributes
            nullptr                // No template file
        ));

        if (!hFile) { // Check if file handle is valid (operator bool)
            DWORD lastError = GetLastError();
            std::string errorMsg = "无法创建/打开日志文件: " + wstring_to_utf8(LOG_PATH) +
                " 错误代码: " + std::to_string(lastError) +
                " (" + std::system_category().message(lastError) + ")\n"; // "Cannot create/open log file: ... Error code: "
            OutputDebugStringA(errorMsg.c_str()); // Log error to debugger
            return; // Cannot write to file
        }

        // Explicitly move to the end of the file before writing
        DWORD dwPtr = SetFilePointer(hFile.get(), 0, NULL, FILE_END);
        if (dwPtr == INVALID_SET_FILE_POINTER) {
            DWORD lastError = GetLastError();
            std::string errorMsg = "无法移动到日志文件末尾: " + wstring_to_utf8(LOG_PATH) +
                " 错误代码: " + std::to_string(lastError) +
                " (" + std::system_category().message(lastError) + ")\n"; // "Cannot seek to end of log file: ... Error code: "
            OutputDebugStringA(errorMsg.c_str());
            return; // Don't write if we can't seek
        }

        DWORD bytesWritten = 0;
        std::string fileLogEntry = timestamp_str + " " + message_utf8 + "\r\n"; // Add CRLF for file

        // Write the log entry to the file
        if (!WriteFile(hFile.get(), fileLogEntry.c_str(), static_cast<DWORD>(fileLogEntry.length()), &bytesWritten, nullptr) ||
            bytesWritten != fileLogEntry.length()) // Also check if all bytes were written
        {
            DWORD lastError = GetLastError();
            std::string errorMsg = "无法写入日志文件: " + wstring_to_utf8(LOG_PATH) +
                " 错误代码: " + std::to_string(lastError) +
                " (" + std::system_category().message(lastError) + ")\n"; // "Cannot write to log file: ... Error code: "
            OutputDebugStringA(errorMsg.c_str()); // Log error to debugger
        }
    } // Mutex and FileHandle released here
}

// Reads the last N lines from the log file (UTF-8) and returns them as wstrings.
// Note: This implementation reads the entire file into memory, which can be
// inefficient for very large log files. A more robust solution for large files
// would involve reading the file backward in chunks.
std::vector<std::wstring> ReadLastLogLinesW(int maxLines) {
    std::vector<std::wstring> resultLines;
    if (maxLines <= 0) {
        return resultLines; // Return empty if no lines requested
    }

    // Open file for reading in binary mode
    // Use RAII for file stream
    std::ifstream logFile(LOG_PATH, std::ios::binary | std::ios::ate); // Open and seek to end
    if (!logFile.is_open()) {
        // Don't log here, as Log() might call this function, causing recursion.
        // OutputDebugStringA(("日志文件未找到或无法打开: " + wstring_to_utf8(LOG_PATH) + "\n").c_str()); // "Log file not found or cannot be opened: "
        return resultLines; // Return empty vector if file can't be opened
    }

    std::streamsize size = logFile.tellg(); // Get file size
    if (size <= 0) {
        // logFile.close(); // Destructor will close
        return resultLines; // Return empty vector for empty file
    }
    logFile.seekg(0, std::ios::beg); // Seek back to the beginning

    // Read the entire file content into a string buffer
    std::string content;
    try {
        content.resize(static_cast<size_t>(size)); // Pre-allocate buffer
        if (!logFile.read(&content[0], size)) {
            // Log read error (avoid calling Log directly)
            OutputDebugStringA(("读取日志文件内容失败: " + wstring_to_utf8(LOG_PATH) + "\n").c_str()); // "Failed to read log file content: "
            return resultLines; // Return empty vector on read error
        }
    }
    catch (const std::bad_alloc& ba) {
        OutputDebugStringA(("内存不足，无法读取日志文件 (" + std::to_string(size) + " bytes): " + std::string(ba.what()) + "\n").c_str()); // "Out of memory reading log file (... bytes): "
        return resultLines;
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("读取日志文件时出错: " + std::string(e.what()) + "\n").c_str()); // "Error reading log file: "
        return resultLines;
    }
    catch (...) {
        OutputDebugStringA("读取日志文件时发生未知错误。\n"); // "Unknown error reading log file."
        return resultLines;
    }
    // logFile is closed automatically by ifstream destructor

    // Convert the UTF-8 content to wstring (UTF-16)
    std::wstring wideContent = utf8_to_wstring(content);
    if (wideContent.empty() && !content.empty()) {
        OutputDebugStringA("警告：日志内容 UTF-8 -> wstring 转换失败。\n"); // "Warning: Log content UTF-8 -> wstring conversion failed."
        // Proceeding might show corrupted data, returning empty is safer.
        return resultLines;
    }

    // Process the wide string line by line
    std::wstringstream wss(wideContent);
    std::wstring line;
    std::deque<std::wstring> lastNLines; // Use deque for efficient front removal

    // Read lines and keep only the last 'maxLines'
    while (getline(wss, line, L'\n')) { // Split by newline
        // Remove trailing carriage return if present (for Windows line endings \r\n)
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        // Only add non-empty lines if desired (optional)
        // if (!line.empty()) {
        lastNLines.push_back(std::move(line)); // Move string
        // Maintain the size limit
        if (lastNLines.size() > static_cast<size_t>(maxLines)) {
            lastNLines.pop_front();
        }
        // }
    }

    // Copy the deque content to the result vector
    try {
        // Use move iterators for efficiency
        resultLines.assign(std::make_move_iterator(lastNLines.begin()),
            std::make_move_iterator(lastNLines.end()));
    }
    catch (const std::bad_alloc& ba) {
        OutputDebugStringA(("内存分配错误，无法创建结果日志行向量: " + std::string(ba.what()) + "\n").c_str()); // "Memory allocation error creating result log line vector: "
        resultLines.clear(); // Ensure vector is empty on error
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("创建结果日志行向量时出错: " + std::string(e.what()) + "\n").c_str()); // "Error creating result log line vector: "
        resultLines.clear();
    }
    catch (...) {
        OutputDebugStringA("创建结果日志行向量时发生未知错误。\n"); // "Unknown error creating result log line vector."
        resultLines.clear();
    }


    return resultLines;
}

// Loads log entries from the in-memory cache and displays them in an Edit control.
void LoadAndDisplayLogs(HWND hEdit) {
    // Check if the Edit control handle is valid
    if (!hEdit || !IsWindow(hEdit)) {
        OutputDebugStringA("LoadAndDisplayLogs: 无效的编辑控件句柄。\n"); // "LoadAndDisplayLogs: Invalid Edit control handle."
        return;
    }

    std::wstringstream wss; // Use a stringstream to build the text efficiently

    // Lock the cache mutex while accessing g_logCache
    try {
        std::lock_guard<std::mutex> cache_lock(g_logCacheMutex);
        // Append each cached log line to the stringstream
        for (const std::wstring& line : g_logCache) {
            wss << line << L"\r\n"; // Add Windows-style line endings for Edit control
        }
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("访问日志缓存时出错: " + std::string(e.what()) + "\n").c_str()); // "Error accessing log cache: "
        // Optionally clear the stringstream or handle error
        wss.str(L""); // Clear potentially partial content
        wss << L"错误：无法加载日志缓存。\r\n"; // "Error: Failed to load log cache."
    }
    catch (...) {
        OutputDebugStringA("访问日志缓存时发生未d知错误。\n"); // "Unknown error accessing log cache."
        wss.str(L"");
        wss << L"错误：无法加载日志缓存。\r\n"; // "Error: Failed to load log cache."
    }
    // Mutex released here

    // Set the text of the Edit control
    // Consider performance for very large log caches - SetWindowTextW can be slow.
    // Alternatives: Send EM_REPLACESEL or manage the control's text buffer more directly.
    if (!SetWindowTextW(hEdit, wss.str().c_str())) {
        DWORD lastError = GetLastError();
        OutputDebugStringA(("LoadAndDisplayLogs: SetWindowTextW 失败。错误代码: " + std::to_string(lastError) + "\n").c_str()); // "LoadAndDisplayLogs: SetWindowTextW failed. Error code: "
    }

    // Scroll the Edit control to the bottom to show the latest entries
    SendMessageW(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
}