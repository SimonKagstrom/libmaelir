#include "hal/i_stepper_motor.hh"

#include <trompeloeil.hpp>

class MockStepperMotor : public hal::IStepperMotor
{
public:
    MAKE_MOCK1(Step, void(int));
};
