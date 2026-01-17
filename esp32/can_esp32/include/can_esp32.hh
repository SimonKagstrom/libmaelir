#pragma once

#include "hal/i_can.hh"

#include <array>
#include <esp_twai.h>
#include <esp_twai_onchip.h>
#include <etl/queue_spsc_atomic.h>

class CanEsp32 : public hal::ICan
{
public:
    CanEsp32(gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate_bps);

    void Start() final;
    void Stop() final;
    bool SendFrame(uint32_t id, std::span<const uint8_t> data) final;
    std::optional<Frame> ReceiveFrame() final;
    std::unique_ptr<ListenerCookie> AttachWakeupListener(os::binary_semaphore& sem) final;

private:
    static constexpr auto kRxQueueSize = 32;

    bool TwaiRxCb(const twai_rx_done_event_data_t* edata);

    twai_node_handle_t m_node_hdl;
    os::binary_semaphore* m_wakeup_semaphore {nullptr};

    std::array<hal::ICan::Frame, kRxQueueSize> m_rx_buffers;
    etl::queue_spsc_atomic<uint8_t, kRxQueueSize> m_rx_free_buffer_indicies;
    etl::queue_spsc_atomic<uint8_t, kRxQueueSize> m_rx_queue;
};
