#include "pm_host.hh"

namespace
{
class PmLockHost final : public hal::IPm::ILock
{
public:
    void Lock() override
    {
    }

    void Unlock() override
    {
    }
};


} // namespace

std::unique_ptr<hal::IPm::ILock>
PmHost::CreateFullPowerLock()
{
    return std::make_unique<PmLockHost>();
}
