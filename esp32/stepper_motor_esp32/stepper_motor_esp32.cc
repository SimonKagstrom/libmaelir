#include "stepper_motor_esp32.hh"

#include <esp_check.h>

// From https://github.com/espressif/esp-idf/blob/release/v5.5/examples/peripherals/rmt/stepper_motor/
namespace
{

static const char* TAG = "stepper_motor_encoder";

typedef struct
{
    rmt_encoder_t base;
    rmt_encoder_handle_t copy_encoder;
    uint32_t resolution;
} rmt_stepper_uniform_encoder_t;

size_t
rmt_encode_stepper_motor_uniform(rmt_encoder_t* encoder,
                                 rmt_channel_handle_t channel,
                                 const void* primary_data,
                                 size_t data_size,
                                 rmt_encode_state_t* ret_state)
{
    rmt_stepper_uniform_encoder_t* motor_encoder =
        __containerof(encoder, rmt_stepper_uniform_encoder_t, base);
    rmt_encoder_handle_t copy_encoder = motor_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    uint32_t target_freq_hz = *(uint32_t*)primary_data;
    uint16_t symbol_duration = motor_encoder->resolution / target_freq_hz / 2;
    rmt_symbol_word_t freq_sample = {
        .duration0 = symbol_duration,
        .level0 = 0,
        .duration1 = symbol_duration,
        .level1 = 1,
    };
    size_t encoded_symbols = copy_encoder->encode(
        copy_encoder, channel, &freq_sample, sizeof(freq_sample), &session_state);
    *ret_state = session_state;
    return encoded_symbols;
}

esp_err_t
rmt_del_stepper_motor_uniform_encoder(rmt_encoder_t* encoder)
{
    rmt_stepper_uniform_encoder_t* motor_encoder =
        __containerof(encoder, rmt_stepper_uniform_encoder_t, base);
    rmt_del_encoder(motor_encoder->copy_encoder);
    free(motor_encoder);
    return ESP_OK;
}

esp_err_t
rmt_reset_stepper_motor_uniform(rmt_encoder_t* encoder)
{
    rmt_stepper_uniform_encoder_t* motor_encoder =
        __containerof(encoder, rmt_stepper_uniform_encoder_t, base);
    rmt_encoder_reset(motor_encoder->copy_encoder);
    return ESP_OK;
}

esp_err_t
rmt_new_stepper_motor_uniform_encoder(uint32_t resolution, rmt_encoder_handle_t* ret_encoder)
{
    assert(ret_encoder);

    esp_err_t ret = ESP_OK;
    auto step_encoder = (rmt_stepper_uniform_encoder_t*)rmt_alloc_encoder_mem(
        sizeof(rmt_stepper_uniform_encoder_t));
    if (!step_encoder)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for stepper uniform encoder");
        return ESP_ERR_NO_MEM;
    }
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &step_encoder->copy_encoder),
                      err,
                      TAG,
                      "create copy encoder failed");

    step_encoder->resolution = resolution;
    step_encoder->base.del = rmt_del_stepper_motor_uniform_encoder;
    step_encoder->base.encode = rmt_encode_stepper_motor_uniform;
    step_encoder->base.reset = rmt_reset_stepper_motor_uniform;
    *ret_encoder = &(step_encoder->base);
    return ESP_OK;
err:
    if (step_encoder)
    {
        if (step_encoder->copy_encoder)
        {
            rmt_del_encoder(step_encoder->copy_encoder);
        }
        free(step_encoder);
    }
    return ret;
}
} // namespace


constexpr auto kClockwiseLevel = true;
constexpr auto kCounterClockwiseLevel = !kClockwiseLevel;
constexpr auto kStepMotorResolutionHz = 1000000; // 1MHz resolution


StepperMotorEsp32::StepperMotorEsp32(hal::IGpio& en_gpio,
                                     hal::IGpio& dir_gpio,
                                     gpio_num_t step_gpio_num)
    : m_en_gpio(en_gpio)
    , m_dir_gpio(dir_gpio)
{
    m_en_gpio.SetState(false);
    m_dir_gpio.SetState(kClockwiseLevel);

    m_tx_channel_config = {
        .gpio_num = step_gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT, // select clock source
        .resolution_hz = kStepMotorResolutionHz,
        .mem_block_symbols = 64,
        .trans_queue_depth =
            10, // set the number of transactions that can be pending in the background
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&m_tx_channel_config, &m_motor_chan));
    ESP_ERROR_CHECK(
        rmt_new_stepper_motor_uniform_encoder(kStepMotorResolutionHz, &m_uniform_motor_encoder));
}

void
StepperMotorEsp32::Start()
{
    m_en_gpio.SetState(true);
    ESP_ERROR_CHECK(rmt_enable(m_motor_chan));
}

void
StepperMotorEsp32::Step(int delta)
{
    m_dir_gpio.SetState(delta > 0 ? kClockwiseLevel : kCounterClockwiseLevel);

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    const static uint32_t uniform_speed_hz = 1500;

    tx_config.loop_count = std::abs(delta);
    ESP_ERROR_CHECK(rmt_transmit(m_motor_chan,
                                 m_uniform_motor_encoder,
                                 &uniform_speed_hz,
                                 sizeof(uniform_speed_hz),
                                 &tx_config));

    ESP_ERROR_CHECK(rmt_tx_wait_all_done(m_motor_chan, -1));
}
