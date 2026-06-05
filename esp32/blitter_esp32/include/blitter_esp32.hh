#pragma once

#include "hal/i_blitter.hh"
#include "semaphore.hh"

#include <atomic>
#include <driver/ppa.h>

// Warning: This class assumes correctly clipped operations are passed to the BlitOperations
class BlitterEsp32 : public hal::IBlitter
{
public:
    BlitterEsp32();

private:
    static constexpr auto kMaxTransactions = 48; // Worst case: all tiles are 1 pixel and we have to blit each pixel separately, plus some extra for partial tiles on the edges

    void BlitOperations(std::span<const hal::BlitOperation> operations) final;
    void WaitForBlitsDone() final;

    void BlitOne(const hal::BlitOperation& op, bool last);
    void OnTransactionDone();

    ppa_client_handle_t m_client {nullptr};

    os::binary_semaphore m_transaction_done_semaphore {0};
    std::atomic<int32_t> m_pending_transactions {0};
};
