#include "blitter_esp32.hh"
#include "debug_assert.hh"

#include <cassert>
#include <esp_cache.h>
#include <utility>


static_assert(std::to_underlying(hal::k0) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_0));
static_assert(std::to_underlying(hal::k90) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_90));
static_assert(std::to_underlying(hal::k180) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_180));
static_assert(std::to_underlying(hal::k270) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_270));

namespace
{

bool
PrepareOperationForPpa(hal::BlitOperation& op)
{
    if (op.src_data == nullptr || op.src_width <= 0 || op.src_height <= 0 || op.width <= 0 ||
        op.height <= 0)
    {
        return false;
    }

    // For k0 we can clip partially visible/source-overlapping rectangles.
    if (op.rotation == hal::k0)
    {
        int32_t sx = op.src_offset_x;
        int32_t sy = op.src_offset_y;
        int32_t dx = op.dst_offset_x;
        int32_t dy = op.dst_offset_y;
        int32_t w = op.width;
        int32_t h = op.height;

        if (dx < 0)
        {
            auto cut = -dx;
            sx += cut;
            w -= cut;
            dx = 0;
        }
        if (dy < 0)
        {
            auto cut = -dy;
            sy += cut;
            h -= cut;
            dy = 0;
        }
        if (sx < 0)
        {
            auto cut = -sx;
            dx += cut;
            w -= cut;
            sx = 0;
        }
        if (sy < 0)
        {
            auto cut = -sy;
            dy += cut;
            h -= cut;
            sy = 0;
        }

        w = std::min(w, static_cast<int32_t>(op.src_width) - sx);
        h = std::min(h, static_cast<int32_t>(op.src_height) - sy);
        w = std::min(w, static_cast<int32_t>(hal::kDisplayWidth) - dx);
        h = std::min(h, static_cast<int32_t>(hal::kDisplayHeight) - dy);

        if (w <= 0 || h <= 0)
        {
            return false;
        }

        op.src_offset_x = static_cast<int16_t>(sx);
        op.src_offset_y = static_cast<int16_t>(sy);
        op.dst_offset_x = static_cast<int16_t>(dx);
        op.dst_offset_y = static_cast<int16_t>(dy);
        op.width = static_cast<int16_t>(w);
        op.height = static_cast<int16_t>(h);
        return true;
    }

    // For rotated blits, keep validation strict to avoid invalid SRM geometry.
    if (op.src_offset_x < 0 || op.src_offset_y < 0 || op.dst_offset_x < 0 || op.dst_offset_y < 0)
    {
        return false;
    }
    if (op.src_offset_x + op.width > op.src_width || op.src_offset_y + op.height > op.src_height)
    {
        return false;
    }

    const int32_t out_w =
        (op.rotation == hal::k90 || op.rotation == hal::k270) ? op.height : op.width;
    const int32_t out_h =
        (op.rotation == hal::k90 || op.rotation == hal::k270) ? op.width : op.height;
    if (op.dst_offset_x + out_w > hal::kDisplayWidth ||
        op.dst_offset_y + out_h > hal::kDisplayHeight)
    {
        return false;
    }

    return true;
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
