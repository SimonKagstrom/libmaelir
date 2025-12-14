#include "hal/i_pm.hh"

class PmEsp32 final : public hal::IPm
{
public:
    std::unique_ptr<hal::IPm::ILock> CreateFullPowerLock() override;
};
