#include "can_esp32.hh"

namespace
{
os::binary_semaphore g_dummy_sem {0};
}

CanEsp32::CanEsp32(gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate_bps)
    : m_wakeup_semaphore(&g_dummy_sem)
{
    twai_onchip_node_config_t node_config = {};

    node_config.io_cfg = {
        .tx = tx_pin,
        .rx = rx_pin,
        .quanta_clk_out = GPIO_NUM_NC,
        .bus_off_indicator = GPIO_NUM_NC,
    };
    node_config.bit_timing.bitrate = baud_rate_bps;
    node_config.tx_queue_depth = 5;

    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &m_node_hdl));

    for (auto i = 0u; i < kRxQueueSize; ++i)
    {
        m_rx_free_buffer_indicies.push(i);
    }

    twai_event_callbacks_t user_cbs = {};
    user_cbs.on_rx_done = [](twai_node_handle_t handle,
                             const twai_rx_done_event_data_t* edata,
                             void* user_ctx) -> bool {
        return static_cast<CanEsp32*>(user_ctx)->TwaiRxCb(edata);
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(m_node_hdl, &user_cbs, this));
}


std::unique_ptr<ListenerCookie>
CanEsp32::Start(os::binary_semaphore& sem)
{
    // Should be stopped already, but make sure
    Stop();
    m_wakeup_semaphore = &sem;

    // Clear pending frames
    uint8_t idx;
    while (m_rx_queue.pop(idx))
    {
        m_rx_free_buffer_indicies.push(idx);
    }
    twai_node_enable(m_node_hdl);

    return std::make_unique<ListenerCookie>([this]() { m_wakeup_semaphore = &g_dummy_sem; });
}

void
CanEsp32::Stop()
{
    twai_node_disable(m_node_hdl);
}

bool
CanEsp32::SendFrame(uint32_t id, std::span<const uint8_t> data)
{
    twai_frame_t tx_msg = {};

    tx_msg.header.id = id;
    tx_msg.header.ide = true; // Use 29-bit extended ID format

    // Non-const, but I sure hope that it doesn't really need to be modified...
    tx_msg.buffer = (uint8_t*)data.data();
    tx_msg.buffer_len = data.size();

    ESP_ERROR_CHECK(twai_node_transmit(m_node_hdl, &tx_msg, 0));
    return twai_node_transmit_wait_all_done(m_node_hdl, -1) == ESP_OK;
}


bool
CanEsp32::TwaiRxCb(const twai_rx_done_event_data_t* edata)
{
    bool rv = false;
    uint8_t idx;

    if (!m_rx_free_buffer_indicies.pop(idx))
    {
        return false;
    }

    auto& frame = m_rx_buffers[idx];
    frame.data.clear(); // Empty if the receive fails, ignore the message

    twai_frame_t rx_frame = {
        .buffer = frame.data.data(),
        .buffer_len = hal::ICan::kMaxDataLength,
    };
    if (twai_node_receive_from_isr(m_node_hdl, &rx_frame) == ESP_OK)
    {
        frame.id = rx_frame.header.id;
        frame.timestamp = milliseconds(rx_frame.header.timestamp / 1000);
        frame.data.uninitialized_resize(rx_frame.header.dlc);

        rv = true;
    }
    m_wakeup_semaphore->release_from_isr();

    m_rx_queue.push(idx);

    return rv;
}


std::optional<hal::ICan::Frame>
CanEsp32::ReceiveFrame()
{
    uint8_t idx;
    if (!m_rx_queue.pop(idx))
    {
        return std::nullopt;
    }

    auto frame = m_rx_buffers[idx];
    m_rx_free_buffer_indicies.push(idx);

    if (frame.data.empty())
    {
        // Some sort of error frame. Maybe signal some other way
        return std::nullopt;
    }

    return frame;
}
