#pragma once

namespace hal
{

class IStepperMotor
{
public:
    enum class Direction
    {
        kClockwise,
        kCounterClockwise,

        kValueCount,
    };

    virtual ~IStepperMotor() = default;

    virtual void Step(Direction direction, unsigned count) = 0;
};

}