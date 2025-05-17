#include "update.h"
#include "network.h" 
#include "log.h"
#include "utils.h"   
#include "globals.h" 
#include "system_ops.h" 

#include <vector>    // For std::vector
#include <sstream>   // For std::wstringstream, std::istringstream
#include <algorithm> // For std::replace, std::max (was missing for std::replace)
#include <stdexcept> // For std::invalid_argument

// For parsing JSON - placeholder. Use a real library.
// #include "json.hpp" 
// using json = nlohmann::json;

namespace Update {

    std::vector<int> ParseVersionString(const std::wstring& versionStr) {
        std::vector<int> parts;
        std::wstringstream wss(versionStr);
        std::wstring segment;
        while (std::getline(wss, segment, L'.')) {
            if (segment.empty() && !wss.eof()) { // Handle cases like "1..2" -> treat empty segment as 0 or error
                parts.push_back(0); // Or throw error
                continue;
            }
            if (segment.empty() && wss.eof() && parts.empty()) { // Handle empty string input
                throw std::invalid_argument("Empty version string segment or overall empty string");
            }
            if (segment.empty()) continue; // Skip trailing empty segments if any e.g. "1.2."

            try {
                parts.push_back(std::stoi(segment));
            }
            catch (const std::invalid_argument& ia) {
                LOG_WARNING(L"Invalid version segment (not an int): '", segment, L"'. Error: ", Utf8ToWide(ia.what()).c_str());
                throw std::invalid_argument("Version segment is not a valid integer");
            }
            catch (const std::out_of_range& oor) {
                LOG_WARNING(L"Version segment out of range: '", segment, L"'. Error: ", Utf8ToWide(oor.what()).c_str());
                throw std::invalid_argument("Version segment out of integer range");
            }
        }
        if (parts.empty() && !versionStr.empty()) { // e.g. versionStr was "..."
            throw std::invalid_argument("Version string contains no valid numeric parts.");
        }
        // No need to pad to 3 parts here, CompareVersions will handle different lengths.
        return parts;
    }

    int CompareVersions(const std::wstring& version1, const std::wstring& version2) {
        if (version1 == version2) return 0;

        std::vector<int> v1_parts, v2_parts;
        try {
            v1_parts = ParseVersionString(version1);
            v2_parts = ParseVersionString(version2);
        }
        catch (const std::invalid_argument& e) {
            LOG_ERROR(L"Cannot compare versions due to parsing error: ", Utf8ToWide(e.what()).c_str());
            throw;
        }

        size_t len = max(v1_parts.size(), v2_parts.size());
        for (size_t i = 0; i < len; ++i) {
            int p1 = (i < v1_parts.size()) ? v1_parts[i] : 0; // Pad with 0 for shorter version strings
            int p2 = (i < v2_parts.size()) ? v2_parts[i] : 0;
            if (p1 < p2) return -1;
            if (p1 > p2) return 1;
        }
        return 0;
    }


    bool CheckForUpdates(
        const std::wstring& currentVersion,
        const std::string& updateCheckUrl, // URL is std::string
        VersionInfo& outVersionInfo)
    {
        LOG_INFO(L"Checking for updates. Current version: ", currentVersion, L". Update URL: ", Utf8ToWide(updateCheckUrl).c_str());

        std::string responseBody;
        Network::ParsedUrl parsedUrl = Network::ParseUrl(updateCheckUrl); // C2589, C2059 fixed by assigning to var first
        if (!parsedUrl.isValid) {
            LOG_ERROR(L"Invalid update check URL: ", Utf8ToWide(updateCheckUrl).c_str());
            return false;
        }

        // Construct full path for HttpGet
        std::string fullPath = parsedUrl.path;
        if (!parsedUrl.query.empty()) {
            fullPath += "?" + parsedUrl.query;
        }


        // C2660: HttpGet takes 7 arguments. The call was missing some or had wrong types.
        // bool HttpGet(const std::string& host, const std::string& path, unsigned short port,
        //              std::string& responseBody, std::map<std::string, std::string>* responseHeadersOutParam = nullptr,
        //              bool useHTTPSParam = false, int timeoutMsParam = 5000)
        if (!Network::HttpGet(parsedUrl.host, fullPath, parsedUrl.port, responseBody,
            nullptr, (parsedUrl.scheme == "https"), 10000 /*timeout 10s*/)) {
            LOG_ERROR(L"Failed to fetch update information from: ", Utf8ToWide(updateCheckUrl).c_str());
            return false;
        }

        if (responseBody.empty()) {
            LOG_WARNING(L"Update check URL returned empty response: ", Utf8ToWide(updateCheckUrl).c_str());
            return false;
        }

        // --- JSON Parsing Placeholder ---
        // This part needs a real JSON library. The manual parsing is extremely fragile.
        // Assuming a simple JSON structure:
        // { "latestVersion": "1.2.3", "downloadUrl": "http://...", "releaseNotes": "..." }
        // For the sake_of_compilation, this pseudo-parser will remain, but it's bad.
        size_t verPos = responseBody.find("\"latestVersion\": \"");
        size_t urlPos = responseBody.find("\"downloadUrl\": \"");
        size_t notesPos = responseBody.find("\"releaseNotes\": \"");

        if (verPos != std::string::npos && urlPos != std::string::npos && notesPos != std::string::npos) {
            verPos += strlen("\"latestVersion\": \"");
            urlPos += strlen("\"downloadUrl\": \"");
            notesPos += strlen("\"releaseNotes\": \"");

            size_t verEnd = responseBody.find("\"", verPos);
            size_t urlEnd = responseBody.find("\"", urlPos);
            size_t notesEnd = responseBody.find("\"", notesPos);

            if (verEnd != std::string::npos && urlEnd != std::string::npos && notesEnd != std::string::npos) {
                outVersionInfo.versionString = Utf8ToWide(responseBody.substr(verPos, verEnd - verPos));
                // C2679: outVersionInfo.downloadUrl is wstring, RHS is string.
                outVersionInfo.downloadUrl = responseBody.substr(urlPos, urlEnd - urlPos); // This is std::string
                outVersionInfo.releaseNotes = Utf8ToWide(responseBody.substr(notesPos, notesEnd - notesPos));
            }
            else {
                LOG_ERROR(L"Failed to parse update information (malformed pseudo-JSON - missing end quotes).");
                LOG_DEBUG(L"Response body: ", Utf8ToWide(responseBody).c_str());
                return false;
            }
        }
        else {
            LOG_ERROR(L"Failed to parse update information (expected fields not found in pseudo-JSON).");
            LOG_DEBUG(L"Response body: ", Utf8ToWide(responseBody).c_str());
            return false;
        }


        if (outVersionInfo.versionString.empty() || outVersionInfo.downloadUrl.empty()) {
            LOG_ERROR(L"Parsed update information is incomplete (missing version or URL string).");
            return false;
        }

        LOG_INFO(L"Latest version available: ", outVersionInfo.versionString, L". Download URL: ", Utf8ToWide(outVersionInfo.downloadUrl).c_str());
        LOG_INFO(L"Release notes: ", outVersionInfo.releaseNotes);

        try {
            if (CompareVersions(outVersionInfo.versionString, currentVersion) > 0) {
                LOG_INFO(L"A new version is available: ", outVersionInfo.versionString);
                return true;
            }
            else {
                LOG_INFO(L"Current version ", currentVersion, L" is up to date or newer.");
                return false;
            }
        }
        catch (const std::invalid_argument& e) {
            LOG_ERROR(L"Error comparing versions: ", Utf8ToWide(e.what()).c_str());
            return false;
        }
    }


    bool DownloadAndApplyUpdate(
        const VersionInfo& versionToUpdate, // versionToUpdate.downloadUrl is std::string
        std::function<void(long long, long long)> progressCallback,
        std::function<bool()> restartAppCallback)
    {
        // versionToUpdate.downloadUrl is std::string as per VersionInfo struct and parsing logic
        if (versionToUpdate.downloadUrl.empty()) {
            LOG_ERROR(L"Download URL for update is empty.");
            return false;
        }

        std::wstring tempDir = g_appDataDir + L"\\Temp";
        if (!DirectoryExists(tempDir)) {
            if (!CreateDirectoryRecursive(tempDir)) {
                LOG_ERROR(L"Failed to create temporary directory for update: ", tempDir.c_str());
                return false;
            }
        }

        std::wstring fileName = L"update_package_default_name.dat"; // Default name
        // versionToUpdate.downloadUrl is std::string. Need to convert to wstring for find_last_of if used with L'/'
        // Or, use std::string version of find_last_of with '/'
        size_t lastSlashUrl = versionToUpdate.downloadUrl.find_last_of('/');
        if (lastSlashUrl != std::string::npos) {
            // C2664: Utf8ToWide expects std::string, versionToUpdate.downloadUrl.substr(...) is std::string. This is OK.
            fileName = Utf8ToWide(versionToUpdate.downloadUrl.substr(lastSlashUrl + 1));
        }

        // Sanitize filename (basic)
        // C2039, C3861 std::replace: needs <algorithm>. It operates on iterators.
        std::replace(fileName.begin(), fileName.end(), L'?', L'_'); // Ok for wstring
        std::replace(fileName.begin(), fileName.end(), L'&', L'_'); // Ok for wstring


        std::wstring downloadedFilePath = tempDir + L"\\" + fileName;

        LOG_INFO(L"Downloading update from: ", Utf8ToWide(versionToUpdate.downloadUrl).c_str(), L" to: ", downloadedFilePath.c_str());

        // C2664: DownloadFile expects const std::string& for URL. versionToUpdate.downloadUrl is already std::string.
        if (!Network::DownloadFile(versionToUpdate.downloadUrl, downloadedFilePath, progressCallback)) {
            LOG_ERROR(L"Failed to download update package from: ", Utf8ToWide(versionToUpdate.downloadUrl).c_str());
            if (FileExists(downloadedFilePath)) {
                DeleteFileW(downloadedFilePath.c_str());
            }
            return false;
        }

        LOG_INFO(L"Update package downloaded successfully: ", downloadedFilePath.c_str());
        LOG_WARNING(L"Update package verification (checksum/signature) is NOT IMPLEMENTED. This is a security risk.");

        LOG_INFO(L"Update package ready at: ", downloadedFilePath.c_str());
        LOG_INFO(L"To apply the update, the application typically needs to restart and run an updater/installer.");

        if (restartAppCallback) {
            LOG_INFO(L"Requesting application restart to apply update...");
            PROCESS_INFORMATION pi;
            if (SystemOps::ExecuteProcess(downloadedFilePath, L"", L"", false, &pi, false)) {
                LOG_INFO(L"Launched updater/installer: ", downloadedFilePath.c_str(), L" PID: ", pi.dwProcessId);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (restartAppCallback()) {
                    LOG_INFO(L"Restart callback initiated successfully.");
                    return true;
                }
                else {
                    LOG_ERROR(L"Restart callback failed to initiate application shutdown.");
                    return false;
                }
            }
            else {
                LOG_ERROR(L"Failed to launch updater/installer: ", downloadedFilePath.c_str());
                return false;
            }
        }
        else {
            LOG_WARNING(L"No restart callback provided. Update downloaded but not applied.");
            return false;
        }
    }

} // namespace Update
