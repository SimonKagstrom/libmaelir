#include "nvm_host.hh"

#include <charconv>
#include <fstream>
#include <iostream>

NvmHost::NvmHost(std::string_view file_name)
    : m_file_name(file_name)
{
    // Read the file (key:value lines). The format doesn't say which values are strings, so
    // values with only digits are numbers, and the rest (also empty ones) strings
    std::ifstream file(m_file_name);
    if (file.is_open())
    {
        std::string line;

        while (std::getline(file, line))
        {
            auto separator = line.find(':');
            if (separator == std::string::npos)
            {
                continue;
            }

            auto key = line.substr(0, separator);
            auto value = std::string_view(line).substr(separator + 1);

            uint32_t number = 0;
            auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
            if (!value.empty() && error == std::errc() && end == value.data() + value.size())
            {
                m_storage_cache[key] = number;
            }
            else
            {
                m_storage_cache_string[key] = std::string(value);
            }
        }

        file.close();
    }
}

void
NvmHost::Commit()
{
    // Store the cache to file
    std::ofstream file(m_file_name);
    if (file.is_open())
    {
        for (const auto& [key, value] : m_storage_cache)
        {
            file << key << ':' << value << '\n';
        }
        for (const auto& [key, value] : m_storage_cache_string)
        {
            file << key << ':' << value << '\n';
        }

        file.close();
    }
}

void
NvmHost::EraseAll()
{
    m_storage_cache.clear();
    m_storage_cache_string.clear();
}

void
NvmHost::EraseKey(const char* key)
{
    m_storage_cache.erase(key);
    m_storage_cache_string.erase(key);
}


std::optional<uint32_t>
NvmHost::GetUint32_t(const char* key)
{
    if (auto it = m_storage_cache.find(key); it != m_storage_cache.end())
    {
        return it->second;
    }

    return std::nullopt;
}

void
NvmHost::SetUint32_t(const char* key, uint32_t value)
{
    m_storage_cache[key] = value;
}

std::optional<std::string>
NvmHost::GetString(const char* key)
{
    if (auto it = m_storage_cache_string.find(key); it != m_storage_cache_string.end())
    {
        return it->second;
    }

    return std::nullopt;
}

void
NvmHost::SetString(const char* key, const std::string_view value)
{
    m_storage_cache_string[key] = std::string(value);
}
