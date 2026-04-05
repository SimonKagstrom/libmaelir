#include "base_thread.hh"
#include "time.hh"

#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

using namespace os;

void
os::detail::WaitThreadExit(ThreadHandle thread)
{
    vTaskDeleteWithCaps(thread->m_task);

    delete thread->m_private_data;
    delete thread;
}

os::ThreadHandle
os::detail::StartThread(const char* name,
                        ThreadCore core,
                        ThreadPriority priority,
                        uint32_t stack_size,
                        const std::function<void()>& thread_loop)
{
    auto prio = std::to_underlying(priority);

    static_assert(tskIDLE_PRIORITY == 0, "FreeRTOS priority assumption broken");
    static_assert(std::to_underlying(ThreadPriority::kLow) > tskIDLE_PRIORITY,
                  "FreeRTOS priority assumption broken");
    assert(prio < configMAX_PRIORITIES);
    assert(prio > 0);

    auto out = new ThreadContext;
    out->m_private_data = new ThreadContext::Data;
    out->m_task = nullptr;
    out->m_private_data->m_thread_loop = thread_loop;

    xTaskCreatePinnedToCore(
        [](void* arg) { static_cast<ThreadHandle>(arg)->m_private_data->m_thread_loop(); },
        name,
        stack_size,
        out,
        prio,
        &out->m_task,
        std::to_underlying(core));
    return out;
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
