#include "config.h"
#include "log.h"     // ���ڼ�¼��־
#include "utils.h"   // �����ַ���ת����
#include <fstream>
#include <sstream>   // ����ת�����ֵ��ַ���
#include <algorithm> // ���� std::transform (ת��ΪСд��)

Config::Config() {
    // ���캯��
}

Config::~Config() {
    // ���������������Ҫ�������������Զ���������
    // if (!m_filePath.empty()) {
    //     Save(m_filePath);
    // }
}

bool Config::Load(const std::wstring& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex); // ��֤�̰߳�ȫ

    m_filePath = filePath;
    m_data.clear(); // ���������

    std::wifstream file(filePath);
    if (!file.is_open()) {
        LOG_WARNING(L"Config file not found or could not be opened: ", filePath);
        // �ļ�����������������������״����У�����Ҫ���������Լ�¼һ�� Info
        // LOG_INFO(L"Config file not found, will use default values and create on save: ", filePath);
        return false; // ���� true����ʾ����ʹ��Ĭ��ֵ
    }

    std::wstring line;
    std::wstring currentSection;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line.empty() || line[0] == L'#' || line[0] == L';') { // �������к�ע��
            continue;
        }

        if (line[0] == L'[' && line.back() == L']') { // Section
            currentSection = Trim(line.substr(1, line.length() - 2));
            // ������ת��ΪСд���Ա㲻���ִ�Сд���� (��ѡ)
            // std::transform(currentSection.begin(), currentSection.end(), currentSection.begin(), ::towlower);
        }
        else { // Key-Value pair
            size_t delimiterPos = line.find(L'=');
            if (delimiterPos != std::wstring::npos) {
                std::wstring key = Trim(line.substr(0, delimiterPos));
                std::wstring value = Trim(line.substr(delimiterPos + 1));

                // ������ת��ΪСд (��ѡ)
                // std::transform(key.begin(), key.end(), key.begin(), ::towlower);

                if (!currentSection.empty() && !key.empty()) {
                    m_data[currentSection][key] = value;
                }
                else if (key.empty() && !currentSection.empty()) {
                    LOG_WARNING(L"Empty key found in section [", currentSection, L"] in config file: ", filePath);
                }
                else {
                    LOG_WARNING(L"Key-value pair found outside of a section or empty key/section: ", line);
                }
            }
            else {
                LOG_WARNING(L"Malformed line in config file (no '='): ", line);
            }
        }
    }

    file.close();
    LOG_INFO(L"Configuration loaded from: ", filePath);
    return true;
}

bool Config::Save(const std::wstring& filePathToSave) {
    std::lock_guard<std::mutex> lock(m_mutex); // ��֤�̰߳�ȫ

    std::wstring targetPath = filePathToSave.empty() ? m_filePath : filePathToSave;
    if (targetPath.empty()) {
        LOG_ERROR(L"Cannot save configuration: No file path specified and no previous path loaded.");
        return false;
    }

    // ȷ��Ŀ¼����
    size_t lastSlash = targetPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring dir = targetPath.substr(0, lastSlash);
        if (!DirectoryExists(dir)) {
            if (!CreateDirectoryRecursive(dir)) {
                LOG_ERROR(L"Failed to create directory for config file: ", dir);
                return false;
            }
        }
    }

    std::wofstream file(targetPath);
    if (!file.is_open()) {
        LOG_ERROR(L"Could not open config file for saving: ", targetPath);
        return false;
    }

    for (const auto& sectionPair : m_data) {
        file << L"[" << sectionPair.first << L"]" << std::endl;
        for (const auto& keyPair : sectionPair.second) {
            file << keyPair.first << L" = " << keyPair.second << std::endl;
        }
        file << std::endl; // �ڽ�֮����ӿ��У�����
    }

    file.close();
    if (file.fail()) { // ���رպ����״̬
        LOG_ERROR(L"Failed to write or close config file properly: ", targetPath);
        return false;
    }

    LOG_INFO(L"Configuration saved to: ", targetPath);
    // ����Ǳ��浽��·�������� m_filePath
    if (!filePathToSave.empty()) {
        m_filePath = filePathToSave;
    }
    return true;
}

std::wstring Config::GetString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // std::wstring lookupSection = section;
    // std::wstring lookupKey = key;
    // std::transform(lookupSection.begin(), lookupSection.end(), lookupSection.begin(), ::towlower); // �������ʱת��Сд
    // std::transform(lookupKey.begin(), lookupKey.end(), lookupKey.begin(), ::towlower);

    auto itSection = m_data.find(section); // or lookupSection
    if (itSection != m_data.end()) {
        auto itKey = itSection->second.find(key); // or lookupKey
        if (itKey != itSection->second.end()) {
            return itKey->second;
        }
    }
    return defaultValue;
}

int Config::GetInt(const std::wstring& section, const std::wstring& key, int defaultValue) const {
    std::wstring strValue = GetString(section, key, L"");
    if (strValue.empty()) {
        return defaultValue;
    }
    try {
        return std::stoi(strValue);
    }
    catch (const std::invalid_argument& ia) {
        LOG_WARNING(L"Invalid integer format for [", section, L"]", key, L" = '", strValue, L"'. Using default value. Error: ", ia.what());
    }
    catch (const std::out_of_range& oor) {
        LOG_WARNING(L"Integer value out of range for [", section, L"]", key, L" = '", strValue, L"'. Using default value. Error: ", oor.what());
    }
    return defaultValue;
}

bool Config::GetBool(const std::wstring& section, const std::wstring& key, bool defaultValue) const {
    std::wstring strValue = GetString(section, key, L"");
    if (strValue.empty()) {
        return defaultValue;
    }
    // ���ַ���ת��ΪСд�Ա�Ƚ�
    std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::towlower);
    if (strValue == L"true" || strValue == L"1" || strValue == L"yes" || strValue == L"on") {
        return true;
    }
    if (strValue == L"false" || strValue == L"0" || strValue == L"no" || strValue == L"off") {
        return false;
    }
    LOG_WARNING(L"Invalid boolean format for [", section, L"]", key, L" = '", GetString(section, key, L""), L"'. Using default value.");
    return defaultValue;
}

void Config::SetString(const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // std::wstring s = section;
    // std::wstring k = key;
    // std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    // std::transform(k.begin(), k.end(), k.begin(), ::towlower);
    m_data[section][key] = value;
}

void Config::SetInt(const std::wstring& section, const std::wstring& key, int value) {
    SetString(section, key, std::to_wstring(value));
}

void Config::SetBool(const std::wstring& section, const std::wstring& key, bool value) {
    SetString(section, key, value ? L"true" : L"false");
}

std::wstring Config::Trim(const std::wstring& str) const {
    const std::wstring whitespace = L" \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (std::wstring::npos == first) {
        return L""; // �ַ���ȫ�ǿհ�
    }
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first + 1));
}
