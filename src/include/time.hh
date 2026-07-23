#include <chrono>

using milliseconds = std::chrono::duration<uint32_t, std::milli>;
using seconds = std::chrono::duration<uint32_t, std::ratio<1>>;
using namespace std::chrono_literals;

namespace os
{

milliseconds GetTimeStamp();

uint32_t GetTimeStampRaw();

void Sleep(milliseconds delay);

} // namespace os
