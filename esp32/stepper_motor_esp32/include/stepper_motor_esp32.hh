#pragma once


#include "hal/i_gpio.hh"
#include "hal/i_stepper_motor.hh"

#include <driver/rmt_encoder.h>
#include <driver/rmt_tx.h>

// Driver for the DRV8825
class StepperMotorEsp32 final : public hal::IStepperMotor
{
public:
    StepperMotorEsp32(hal::IGpio& en_gpio, hal::IGpio& dir_gpio, gpio_num_t step_gpio_num);

    void Start();

private:
    void Step(Direction direction, unsigned count) override;

    hal::IGpio& m_en_gpio;
    hal::IGpio& m_dir_gpio;

    rmt_encoder_handle_t m_encoder;
    rmt_tx_channel_config_t m_tx_channel_config {};
    rmt_channel_handle_t m_motor_chan {nullptr};
    rmt_encoder_handle_t m_uniform_motor_encoder{nullptr};
};
