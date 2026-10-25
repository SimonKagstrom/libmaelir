#pragma once

#include "../i_display_brightness.hh"
#include "test.hh"

class MockDisplayBrightness : public hal::IDisplayBrightness
{
public:
    MAKE_MOCK1(SetBrightness, void(uint8_t), override);
};
