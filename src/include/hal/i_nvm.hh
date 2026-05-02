#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace hal
{

class INvm
{
public:
    virtual ~INvm() = default;

    template <typename T>
    std::optional<T> Get(const char* key)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            return GetString(key);
        }
        else
        {
            static_assert(sizeof(T) <= sizeof(uint32_t), "T must fit in a uint32_t");
            static_assert(std::is_trivially_copyable_v<T>,
                          "T must be trivially copyable for INvm::Get");

            if (auto raw = GetUint32_t(key); raw)
            {
                T value = {};

                memcpy(&value, &*raw, sizeof(T));

                return value;
            }

            return std::nullopt;
        }
    }

    template <typename T>
    void Set(const char* key, const T& value)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            SetString(key, value);
        }
        else
        {
            static_assert(sizeof(T) <= sizeof(uint32_t), "T must fit in a uint32_t");
            static_assert(std::is_trivially_copyable_v<T>,
                          "T must be trivially copyable for INvm::Set");

            uint32_t raw = 0;

            memcpy(&raw, &value, sizeof(T));

            SetUint32_t(key, raw);
        }
    }

    virtual void Commit() = 0;

    virtual void EraseAll() = 0;

    virtual void EraseKey(const char* key) = 0;

protected:
    virtual std::optional<uint32_t> GetUint32_t(const char* key) = 0;
    virtual void SetUint32_t(const char* key, uint32_t value) = 0;

    virtual std::optional<std::string> GetString(const char* key) = 0;
    virtual void SetString(const char* key, const std::string_view value) = 0;
};

} // namespace hal
