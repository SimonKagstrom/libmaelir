#pragma once

#include "i_display_properties.hh"
#include "i_display_rotation.hh"

#include <cstdint>
#include <optional>

namespace hal
{
class IDisplay
{
public:
    enum class Owner
    {
        kSoftware,
        kHardware,

        // For translations during blit
        kRotationBuffer,

        kValueCount,
    };

    virtual ~IDisplay() = default;

    /**
     * @brief Return a pointer to the frame buffer.
     *
     * @param owner the owner of the frame buffer
     *
     * @return the frame buffer, or nullptr if it doesn't exist
     */
    virtual uint16_t* GetFrameBuffer(Owner owner) = 0;

    // Inspired by SDL2
    virtual void Flip() = 0;

    void Enable()
    {
        SetActive(true);
    }

    void Disable()
    {
        SetActive(false);
    }

protected:
    virtual void SetActive(bool active) = 0;
};

} // namespace hal
