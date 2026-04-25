#include "blitter_host.hh"

#include "hal/i_display.hh"
#include "painter.hh"

#include <cassert>
#include <utility>

BlitterHost::BlitterHost()
{
}


void
BlitterHost::BlitOne(const hal::BlitOperation& op)
{
    // After rotation, the output rectangle is out_width x out_height in the destination buffer.
    // For 90/270 the axes are swapped relative to the source block.
    int32_t out_width = op.width;
    int32_t out_height = op.height;
    if (op.rotation == hal::Rotation::k90 || op.rotation == hal::Rotation::k270)
    {
        std::swap(out_width, out_height);
    }

    // Destination picture dimensions: for non-rotated blits (tile maps) the output picture is
    // kDisplayWidth x kDisplayHeight. For a full-screen rotated blit the output picture is the
    // physical panel size, which is src_height x src_width (axes swapped). We derive the dst
    // picture width from dst_offset + out_width <= dst_pic_w, using the op's output dimensions.
    const int32_t dst_pic_w =
        (op.rotation != hal::Rotation::k0) ? op.src_height : hal::kDisplayWidth;
    const int32_t dst_pic_h =
        (op.rotation != hal::Rotation::k0) ? op.src_width : hal::kDisplayHeight;

    for (int32_t line = 0; line < out_height; ++line)
    {
        uint32_t dst_y = op.dst_offset_y + line;

        if (dst_y >= static_cast<uint32_t>(dst_pic_h))
        {
            continue;
        }

        for (int32_t x = 0; x < out_width; ++x)
        {
            uint32_t dst_x = op.dst_offset_x + x;

            if (dst_x >= static_cast<uint32_t>(dst_pic_w))
            {
                continue;
            }

            int32_t src_block_x = 0;
            int32_t src_block_y = 0;

            switch (op.rotation)
            {
            case hal::Rotation::k90:
                src_block_x = line;
                src_block_y = op.height - 1 - x;
                break;
            case hal::Rotation::k180:
                src_block_x = op.width - 1 - x;
                src_block_y = op.height - 1 - line;
                break;
            case hal::Rotation::k270:
                src_block_x = op.width - 1 - line;
                src_block_y = x;
                break;
            case hal::Rotation::k0:
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

            if (src_x >= op.src_width || src_y >= op.src_height)
            {
                continue;
            }

            auto src_color = op.src_data[src_y * op.src_width + src_x];
            op.dst_data[dst_y * dst_pic_w + dst_x] = src_color;
        }
    }
}

void
BlitterHost::BlitOperations(std::span<const hal::BlitOperation> operations)
{
    for (const auto& op : operations)
    {
        if (op.height <= 0 || op.width <= 0 || op.dst_offset_x >= hal::kDisplayWidth ||
            op.dst_offset_y >= hal::kDisplayHeight)
        {
            continue;
        }

        BlitOne(op);
    }
}
