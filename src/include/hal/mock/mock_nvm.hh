#pragma once

#include "../i_nvm.hh"

#include <trompeloeil/mock.hpp>

class MockNvm final : public hal::INvm
{
public:
    MAKE_MOCK0(Commit, void(), override);
    MAKE_MOCK0(EraseAll, void(), override);
    MAKE_MOCK1(EraseKey, void(const char* key), override);
    MAKE_MOCK1(GetUint32_t, std::optional<uint32_t>(const char* key), override);
    MAKE_MOCK2(SetUint32_t, void(const char* key, uint32_t value), override);
    MAKE_MOCK1(GetString, std::optional<std::string>(const char* key), override);
    MAKE_MOCK2(SetString, void(const char* key, const std::string_view value), override);
};
