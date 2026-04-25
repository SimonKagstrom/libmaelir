#include "blitter_host.hh"

#include "hal/i_display.hh"
#include "painter.hh"

#include <cassert>
#include <utility>

BlitterHost::BlitterHost()
{
}


void
BlitterHost::BlitOne(uint16_t* frame_buffer, const hal::BlitOperation& op)
{
    int32_t out_width = op.width;
    int32_t out_height = op.height;
    if (op.rotation == hal::k90 || op.rotation == hal::k270)
    {
        std::swap(out_width, out_height);
    }

    for (int32_t line = 0; line < out_height; ++line)
    {
        uint32_t dst_y = op.dst_offset_y + line;

        if (dst_y >= hal::kDisplayHeight)
        {
            continue;
        }

        for (int32_t x = 0; x < out_width; ++x)
        {
            uint32_t dst_x = op.dst_offset_x + x;

            if (dst_x >= hal::kDisplayWidth)
            {
                continue;
            }

            int32_t src_block_x = 0;
            int32_t src_block_y = 0;

            switch (op.rotation)
            {
            case hal::k90:
                src_block_x = line;
                src_block_y = op.height - 1 - x;
                break;
            case hal::k180:
                src_block_x = op.width - 1 - x;
                src_block_y = op.height - 1 - line;
                break;
            case hal::k270:
                src_block_x = op.width - 1 - line;
                src_block_y = x;
                break;
            case hal::k0:
            default:
                src_block_x = x;
                src_block_y = line;
                break;
            }

            int32_t src_x = op.src_offset_x + src_block_x;
            int32_t src_y = op.src_offset_y + src_block_y;

            if (src_x < 0 || src_y < 0)
            {
                continue;
            }

            if (src_x >= static_cast<uint32_t>(op.src_width) ||
                src_y >= static_cast<uint32_t>(op.src_height))
            {
                continue;
            }

            auto src_color = op.src_data[src_y * op.src_width + src_x];
            frame_buffer[dst_y * hal::kDisplayWidth + dst_x] = src_color;
        }
    }
}

void
BlitterHost::BlitOperations(uint16_t* frame_buffer, std::span<const hal::BlitOperation> operations)
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
}
