#include "base_thread.hh"
#include "time.hh"

#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

using namespace os;

os::ThreadHandle
os::detail::CreateThread(const std::function<void()> &thread_loop)
{
    auto out = new ThreadContext;
    out->m_task = nullptr;
    out->m_thread_loop = thread_loop;

    return out;
}

void
os::detail::WaitThreadExit(ThreadHandle thread)
{
    vTaskDeleteWithCaps(thread->m_task);

    delete thread;
}

void
os::detail::StartThread(ThreadHandle thread,
                        const char* name,
                        ThreadCore core,
                        ThreadPriority priority,
                        uint32_t stack_size)
{
    auto prio = std::to_underlying(priority);

    static_assert(tskIDLE_PRIORITY == 0, "FreeRTOS priority assumption broken");
    static_assert(std::to_underlying(ThreadPriority::kLow) > tskIDLE_PRIORITY,
                  "FreeRTOS priority assumption broken");
    assert(prio < configMAX_PRIORITIES);
    assert(prio > 0);

    xTaskCreatePinnedToCore([](void* arg) { static_cast<ThreadHandle>(arg)->m_thread_loop(); },
                            name,
                            stack_size,
                            thread,
                            prio,
                            &thread->m_task,
                            std::to_underlying(core));
}


uint32_t
os::GetTimeStampRaw()
{
    return pdTICKS_TO_MS(xTaskGetTickCount());
}

milliseconds
os::GetTimeStamp()
{
    auto ms_count = os::GetTimeStampRaw();

    return milliseconds(ms_count);
}

void
os::Sleep(milliseconds delay)
{
    vTaskDelay(pdMS_TO_TICKS(delay.count()));
}
