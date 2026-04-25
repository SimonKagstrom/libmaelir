#pragma once

#include "hal/i_blitter.hh"

class BlitterHost : public hal::IBlitter
{
public:
    BlitterHost();

private:
    void BlitOperations(uint16_t* frame_buffer,
                        std::span<const hal::BlitOperation> operations) final;

    void BlitOne(uint16_t* frame_buffer, const hal::BlitOperation& op);
};
