#pragma once

namespace hal
{

class IStepperMotor
{
public:
    virtual ~IStepperMotor() = default;

    virtual void Step(int delta) = 0;
};

}