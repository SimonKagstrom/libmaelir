#pragma once

#include "hal/i_blitter.hh"

class BlitterHost : public hal::IBlitter
{
public:
    BlitterHost();

private:
    void BlitOperations(std::span<const hal::BlitOperation> operations) final;

    void BlitOne(const hal::BlitOperation& op);
};
