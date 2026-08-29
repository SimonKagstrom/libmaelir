#pragma once

#include "../i_gps.hh"

class MockGps : public hal::IGps
{
public:
    virtual ~MockGps() = default;

    MAKE_MOCK1(WaitForData, (std::optional<hal::RawGpsData>)(IEventNotifier & notifier), override);
};
