#pragma once

#include "hal/i_blitter.hh"
#include "semaphore.hh"

#include <atomic>
#include <driver/ppa.h>
#include <etl/vector.h>

class BlitterEsp32 : public hal::IBlitter
{
public:
    BlitterEsp32();

private:
    static constexpr auto kTileSize = 256;
    static constexpr auto kTilePixelCount = kTileSize * kTileSize;
    static constexpr auto kMaxTransactions =
        (hal::kDisplayWidth * hal::kDisplayHeight) / kTilePixelCount +
        hal::kDisplayWidth / kTileSize + 1 + hal::kDisplayHeight / kTileSize +
        8; // Worst case: all tiles are 1 pixel and we have to blit each pixel separately, plus some extra for partial tiles on the edges

    void BlitOperations(std::span<const hal::BlitOperation> operations) final;

    void BlitOne(const hal::BlitOperation& op, bool last);

    bool OnTransactionDone();

    ppa_client_handle_t m_client {nullptr};
    etl::vector<hal::BlitOperation, kMaxTransactions> m_prepared_operations;
};
