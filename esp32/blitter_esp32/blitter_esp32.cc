#include "blitter_esp32.hh"

#include "debug_assert.hh"

#include <algorithm>
#include <cassert>
#include <esp_cache.h>
#include <utility>


static_assert(std::to_underlying(hal::Rotation::k0) ==
              std::to_underlying(PPA_SRM_ROTATION_ANGLE_0));
static_assert(std::to_underlying(hal::Rotation::k90) ==
              std::to_underlying(PPA_SRM_ROTATION_ANGLE_90));
static_assert(std::to_underlying(hal::Rotation::k180) ==
              std::to_underlying(PPA_SRM_ROTATION_ANGLE_180));
static_assert(std::to_underlying(hal::Rotation::k270) ==
              std::to_underlying(PPA_SRM_ROTATION_ANGLE_270));

namespace
{

std::pair<uint32_t, uint32_t>
GetDestinationFramebufferSize(hal::Rotation rotation)
{
    switch (rotation)
    {
    case hal::Rotation::k90:
    case hal::Rotation::k270:
        return {static_cast<uint32_t>(hal::kDisplayHeight),
                static_cast<uint32_t>(hal::kDisplayWidth)};
    case hal::Rotation::k0:
    case hal::Rotation::k180:
    default:
        return {static_cast<uint32_t>(hal::kDisplayWidth),
                static_cast<uint32_t>(hal::kDisplayHeight)};
    }
}

std::pair<uint32_t, uint32_t>
GetDestinationPictureSize(const hal::BlitOperation& op)
{
    if (op.dst_stride > 0 && op.dst_height > 0)
    {
        return {static_cast<uint32_t>(op.dst_stride), static_cast<uint32_t>(op.dst_height)};
    }

    return GetDestinationFramebufferSize(op.rotation);
}

size_t
GetDestinationBufferSizeBytes(const hal::BlitOperation& op)
{
    auto [dst_pic_w, dst_pic_h] = GetDestinationPictureSize(op);
    return static_cast<size_t>(dst_pic_w) * static_cast<size_t>(dst_pic_h) * sizeof(uint16_t);
}

} // namespace

BlitterEsp32::BlitterEsp32()
{
    ppa_client_config_t cfg {};

    cfg.max_pending_trans_num = kMaxTransactions;
    cfg.oper_type = PPA_OPERATION_SRM;
    cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;

    auto res = ppa_register_client(&cfg, &m_client);
    assert(res == ESP_OK);

    auto event_callbacks = ppa_event_callbacks_t {
        .on_trans_done = [](ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data) {
            auto* self = static_cast<BlitterEsp32*>(user_data);
            self->OnTransactionDone();

            return false;
        },
    };
    ppa_client_register_event_callbacks(m_client, &event_callbacks);
}


void
BlitterEsp32::BlitOne(const hal::BlitOperation& op, bool last)
{
    ppa_srm_rotation_angle_t angle =
        static_cast<ppa_srm_rotation_angle_t>(std::to_underlying(op.rotation));
    auto [dst_pic_w, dst_pic_h] = GetDestinationPictureSize(op);
    const uint32_t src_pic_w =
        static_cast<uint32_t>(op.src_stride > 0 ? op.src_stride : op.src_width);

    ppa_srm_oper_config_t cfg = {
        .in =
            {
                // Source tiles are rectangular: src_width x src_height
                .buffer = const_cast<void*>(static_cast<const void*>(op.src_data)),
                .pic_w = src_pic_w,
                .pic_h = static_cast<uint32_t>(op.src_height),
                .block_w = static_cast<uint32_t>(op.width),
                .block_h = static_cast<uint32_t>(op.height),
                .block_offset_x = static_cast<uint32_t>(op.src_offset_x),
                .block_offset_y = static_cast<uint32_t>(op.src_offset_y),
                .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
            },
        .out =
            {
                .buffer = static_cast<void*>(op.dst_data),
                .buffer_size = static_cast<uint32_t>(GetDestinationBufferSizeBytes(op)),
                .pic_w = dst_pic_w,
                .pic_h = dst_pic_h,
                .block_offset_x = static_cast<uint32_t>(op.dst_offset_x),
                .block_offset_y = static_cast<uint32_t>(op.dst_offset_y),
                .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
            },
        .rotation_angle = angle,
        .scale_x = 1,
        .scale_y = 1,
        .mirror_x = false,
        .mirror_y = false,
        .rgb_swap = false,
        .byte_swap = false,
        .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .alpha_fix_val = 0,
        .mode = PPA_TRANS_MODE_NON_BLOCKING,
        .user_data = this,
    };

    auto ret = ppa_do_scale_rotate_mirror(m_client, &cfg);
    assert(ret == ESP_OK);
}

void
BlitterEsp32::BlitOperations(std::span<const hal::BlitOperation> operations)
{
    if (operations.empty())
    {
        return;
    }

    uint16_t* dst_buffer = operations.front().dst_data;
    const size_t dst_buffer_bytes = GetDestinationBufferSizeBytes(operations.front());

    m_pending_transactions += static_cast<uint32_t>(operations.size());
    for (size_t i = 0; i < operations.size(); ++i)
    {
        BlitOne(operations[i], i == operations.size() - 1);
    }
}

void
BlitterEsp32::OnTransactionDone()
{
    const int32_t previous = m_pending_transactions.fetch_sub(1, std::memory_order_acq_rel);
    if (previous == 1)
    {
        m_transaction_done_semaphore.release();
    }
}

void
BlitterEsp32::WaitForBlitsDone()
{
    if (m_pending_transactions.load() > 0)
    {
        m_transaction_done_semaphore.acquire();
    }
}
