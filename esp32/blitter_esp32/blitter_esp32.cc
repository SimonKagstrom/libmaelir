#include "blitter_esp32.hh"

#include "hal/i_display.hh"

#include <cassert>
#include <esp_cache.h>
#include <utility>


static_assert(std::to_underlying(hal::k0) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_0));
static_assert(std::to_underlying(hal::k90) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_90));
static_assert(std::to_underlying(hal::k180) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_180));
static_assert(std::to_underlying(hal::k270) == std::to_underlying(PPA_SRM_ROTATION_ANGLE_270));

BlitterEsp32::BlitterEsp32()
{
    ppa_client_config_t cfg {};

    cfg.max_pending_trans_num = 1;
    cfg.oper_type = PPA_OPERATION_SRM;
    cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;

    auto res = ppa_register_client(&cfg, &m_client);
    assert(res == ESP_OK);
}


void
BlitterEsp32::BlitOne(uint16_t* frame_buffer, const hal::BlitOperation& op)
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
        .mode = PPA_TRANS_MODE_BLOCKING,
        .user_data = nullptr,
    };

    auto ret = ppa_do_scale_rotate_mirror(m_client, &cfg);
    assert(ret == ESP_OK);
}

void
BlitterEsp32::BlitOperations(uint16_t* frame_buffer, std::span<const hal::BlitOperation> operations)
{
    for (const auto& op : operations)
    {
        if (op.height <= 0 || op.width <= 0 || op.dst_offset_x >= hal::kDisplayWidth ||
            op.dst_offset_y >= hal::kDisplayHeight)
        {
            continue;
        }

        BlitOne(frame_buffer, op);
    }

    esp_cache_msync(
        frame_buffer,
        static_cast<size_t>(hal::kDisplayWidth * hal::kDisplayHeight * sizeof(uint16_t)),
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}


#if 0
    for (const auto& o : operations)
    {
        if (o.height < 0 || o.dst_offset_x >= hal::kDisplayWidth ||
            o.dst_offset_y >= hal::kDisplayHeight)
        {
            continue;
        }

        static ppa_srm_oper_config_t cfg = {
            .in =
                {
                    .buffer = (void*)o.src_data,
                    //                    .pic_w = hal::kDisplayWidth,
                    //                    .pic_h = hal::kDisplayHeight,
                    .pic_w = static_cast<uint32_t>(o.src_width),
                    .pic_h = static_cast<uint32_t>(o.src_width),
                    .block_w = static_cast<uint32_t>(o.width),
                    .block_h = static_cast<uint32_t>(o.height),
                    .block_offset_x = static_cast<uint32_t>(o.src_offset_x),
                    .block_offset_y = static_cast<uint32_t>(o.src_offset_y),
                    .srm_cm =  PPA_SRM_COLOR_MODE_RGB565,
                },

            .out =
                {
                    .buffer = (void*)frame_buffer,
                    .buffer_size = hal::kDisplayWidth * hal::kDisplayHeight * 2,
                    .pic_w = hal::kDisplayWidth,
                    .pic_h = hal::kDisplayHeight,
                    .block_offset_x = static_cast<uint32_t>(o.dst_offset_x),
                    .block_offset_y = static_cast<uint32_t>(o.dst_offset_y),
                    .srm_cm =  PPA_SRM_COLOR_MODE_RGB565,
                },
            .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
            .scale_x = 1,
            .scale_y = 1,
            .mirror_x = false,
            .mirror_y = false,
            .rgb_swap = false,
            .byte_swap = false,
            .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
            .alpha_fix_val = 0,
            .mode = PPA_TRANS_MODE_BLOCKING,
            .user_data = static_cast<void*>(this),
        };

        esp_err_t ret = ppa_do_scale_rotate_mirror(m_client, &cfg);
        if (ret != ESP_OK)
        {
            printf("PPA ppa_do_scale etc failed: %d", ret);
        }
    }

#define PPA_ALIGN_UP(x, align) ((((x) + (align) - 1) / (align)) * (align))
#define PPA_PTR_ALIGN_UP(p, align)                                                                 \
    ((void*)(((uintptr_t)(p) + (uintptr_t)((align) - 1)) & ~(uintptr_t)((align) - 1)))

#define PPA_ALIGN_DOWN(x, align) ((((x) - (align) - 1) / (align)) * (align))
#define PPA_PTR_ALIGN_DOWN(p, align)                                                               \
    ((void*)(((uintptr_t)(p) - (uintptr_t)((align) - 1)) & ~(uintptr_t)((align) - 1)))
    esp_cache_msync((void*)PPA_PTR_ALIGN_DOWN(frame_buffer, CONFIG_CACHE_L1_CACHE_LINE_SIZE),
                    PPA_ALIGN_DOWN(hal::kDisplayWidth * hal::kDisplayHeight * 2,
                                   CONFIG_CACHE_L1_CACHE_LINE_SIZE),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
#endif
