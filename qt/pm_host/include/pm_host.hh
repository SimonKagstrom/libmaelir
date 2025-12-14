#pragma once

#include "hal/i_pm.hh"

class PmHost final : public hal::IPm
{
public:
    std::unique_ptr<hal::IPm::ILock> CreateFullPowerLock() override;
};