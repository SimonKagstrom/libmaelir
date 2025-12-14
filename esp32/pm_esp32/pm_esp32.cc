#include "pm_esp32.hh"

#include <esp_pm.h>

namespace
{

class PmLockEsp32 final : public hal::IPm::ILock
{
public:
    PmLockEsp32()
    {
        esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, NULL, &m_lock);
    }

    ~PmLockEsp32() override
    {
        esp_pm_lock_delete(m_lock);
    }

    void Lock() override
    {
        esp_pm_lock_acquire(m_lock);
    }

    void Unlock() override
    {
        esp_pm_lock_release(m_lock);
    }

private:
    esp_pm_lock_handle_t m_lock;
};

} // namespace

std::unique_ptr<hal::IPm::ILock>
PmEsp32::CreateFullPowerLock()
{
    return std::make_unique<PmLockEsp32>();
}
