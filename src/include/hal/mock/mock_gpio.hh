#include "hal/i_gpio.hh"

#include <trompeloeil.hpp>

class MockGpio : public hal::IGpio
{
public:
    MAKE_CONST_MOCK0(GetState, bool());
    MAKE_MOCK1(SetState, void(bool));
    MAKE_MOCK1(AttachIrqListener, std::unique_ptr<hal::ListenerCookie>(std::function<void(bool)>));
};
