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

size_t
GetDestinationBufferSizeBytes(const hal::BlitOperation& op)
{
    auto [dst_pic_w, dst_pic_h] = GetDestinationFramebufferSize(op.rotation);
    return static_cast<size_t>(dst_pic_w) * static_cast<size_t>(dst_pic_h) * sizeof(uint16_t);
}

} // namespace blitter_esp32_internal

BlitterEsp32::BlitterEsp32()
{
    ppa_client_config_t cfg {};

    cfg.max_pending_trans_num = kMaxTransactions;
    cfg.oper_type = PPA_OPERATION_SRM;
    cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;

    auto res = ppa_register_client(&cfg, &m_client);
    assert(res == ESP_OK);
}


void
BlitterEsp32::BlitOne(const hal::BlitOperation& op, bool last)
{
    ppa_srm_rotation_angle_t angle =
        static_cast<ppa_srm_rotation_angle_t>(std::to_underlying(op.rotation));
    auto [dst_pic_w, dst_pic_h] = GetDestinationFramebufferSize(op.rotation);

    ppa_srm_oper_config_t cfg = {
        .in =
            {
                // Source tiles are rectangular: src_width x src_height
                .buffer = const_cast<void*>(static_cast<const void*>(op.src_data)),
                .pic_w = static_cast<uint32_t>(op.src_width),
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
        // Block on the last transaction
        .mode = last ? PPA_TRANS_MODE_BLOCKING : PPA_TRANS_MODE_NON_BLOCKING,
        .user_data = nullptr,
    };

    auto ret = ppa_do_scale_rotate_mirror(m_client, &cfg);
    assert(ret == ESP_OK);
}

void
BlitterEsp32::BlitOperations(std::span<const hal::BlitOperation> operations)
{
    uint16_t* frame_buffer = nullptr;

    m_prepared_operations.clear();

    for (const auto& op : operations)
    {
        m_prepared_operations.push_back(op);
        frame_buffer = reinterpret_cast<uint16_t*>(op.dst_data);
    }

    if (m_prepared_operations.empty())
    {
        return;
    }

    for (size_t i = 0; i < m_prepared_operations.size(); ++i)
    {
        BlitOne(m_prepared_operations[i], i == m_prepared_operations.size() - 1);
    }

    // For now this syncs the entire screen
    esp_cache_msync(
        frame_buffer,
        static_cast<size_t>(hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t)),
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}
