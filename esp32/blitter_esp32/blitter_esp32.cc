#include "blitter_esp32.hh"
#include "debug_assert.hh"

#include <algorithm>
#include <cassert>
#include <esp_cache.h>
#include <utility>


static_assert(std::to_underlying(hal::Rotation::k0) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_0));
static_assert(std::to_underlying(hal::Rotation::k90) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_90));
static_assert(std::to_underlying(hal::Rotation::k180) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_180));
static_assert(std::to_underlying(hal::Rotation::k270) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_270));

bool
PrepareOperationForPpa(hal::BlitOperation& op)
{
    if (op.src_data == nullptr || op.src_width <= 0 || op.src_height <= 0 || op.width <= 0 ||
        op.height <= 0)
    {
        return false;
    }

    // Rotation is only expected for a single full-screen transaction.
    if (op.rotation != hal::Rotation::k0)
    {
        debug_assert(op.src_offset_x == 0 && op.src_offset_y == 0 && op.dst_offset_x == 0 &&
            op.dst_offset_y == 0);
        debug_assert(op.width == hal::kDisplayWidth && op.height == hal::kDisplayHeight);

        if (op.src_width < op.width || op.src_height < op.height)
        {
            return false;
        }

        return true;
    }

    // k0 path: use painter-like clipping against destination and source bounds.
    if (op.src_offset_x < 0 || op.src_offset_y < 0)
    {
        return false;
    }

    int32_t from_x = 0;
    int32_t from_y = 0;
    int32_t width = op.width;
    int32_t height = op.height;

    if (op.dst_offset_x < 0)
    {
        from_x += -op.dst_offset_x;
        width += op.dst_offset_x;
    }
    if (op.dst_offset_y < 0)
    {
        from_y += -op.dst_offset_y;
        height += op.dst_offset_y;
    }

    int32_t dst_x = std::max<int32_t>(0, op.dst_offset_x);
    int32_t dst_y = std::max<int32_t>(0, op.dst_offset_y);
    int32_t src_x = op.src_offset_x + from_x;
    int32_t src_y = op.src_offset_y + from_y;

    width = std::min(width, static_cast<int32_t>(op.src_width) - src_x);
    height = std::min(height, static_cast<int32_t>(op.src_height) - src_y);
    width = std::min(width, static_cast<int32_t>(hal::kDisplayWidth) - dst_x);
    height = std::min(height, static_cast<int32_t>(hal::kDisplayHeight) - dst_y);

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    op.src_offset_x = static_cast<int16_t>(src_x);
    op.src_offset_y = static_cast<int16_t>(src_y);
    op.dst_offset_x = static_cast<int16_t>(dst_x);
    op.dst_offset_y = static_cast<int16_t>(dst_y);
    op.width = static_cast<int16_t>(width);
    op.height = static_cast<int16_t>(height);

    return true;
}

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
BlitterEsp32::BlitOne(uint16_t* frame_buffer, const hal::BlitOperation& op, bool last)
{
    ppa_srm_rotation_angle_t angle =
        static_cast<ppa_srm_rotation_angle_t>(std::to_underlying(op.rotation));

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
                .buffer = static_cast<void*>(frame_buffer),
                .buffer_size = static_cast<uint32_t>(hal::kDisplayWidth * hal::kDisplayHeight *
                                                     sizeof(uint16_t)),
                .pic_w = static_cast<uint32_t>(hal::kDisplayWidth),
                .pic_h = static_cast<uint32_t>(hal::kDisplayHeight),
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
BlitterEsp32::BlitOperations(uint16_t* frame_buffer, std::span<const hal::BlitOperation> operations)
{
    m_prepared_operations.clear();

    for (const auto& op : operations)
    {
        auto prepared = op;
        if (!PrepareOperationForPpa(prepared))
        {
            continue;
        }

        m_prepared_operations.push_back(prepared);
    }

    for (size_t i = 0; i < m_prepared_operations.size(); ++i)
    {
        BlitOne(frame_buffer, m_prepared_operations[i], i == m_prepared_operations.size() - 1);
    }

    esp_cache_msync(
        frame_buffer,
        static_cast<size_t>(hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t)),
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}
