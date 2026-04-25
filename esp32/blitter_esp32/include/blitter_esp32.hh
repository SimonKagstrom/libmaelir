#pragma once

#include "hal/i_blitter.hh"
#include "semaphore.hh"

#include <driver/ppa.h>

class BlitterEsp32 : public hal::IBlitter
{
public:
    BlitterEsp32();

private:
    void BlitOperations(uint16_t* frame_buffer,
                        std::span<const hal::BlitOperation> operations) final;

    void BlitOne(uint16_t* frame_buffer, const hal::BlitOperation& op);

    ppa_client_handle_t m_client {nullptr};
    os::binary_semaphore m_blit_done {0};
};
