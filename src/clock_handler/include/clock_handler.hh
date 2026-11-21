#pragma once

#include "application_state.hh"
#include "messages.hh"
#include "pooled_thread_base.hh"
#include "post_office.hh"

#include <memory>

/**
 * Keeps AS::clock (UTC, seconds since the epoch) ticking once a second. AS::clock stays 0 until
 * a time has been received.
 *
 * The time comes from AS::gps_utc and the network time messages. While there is a fix, the
 * clock follows AS::gps_utc (updated every second), and the network time only sets the
 * timezone. Without a fix, or a GPS time, the network time is used, and the clock ticks on its
 * own.
 */
class ClockHandler : public PooledThreadBase
{
public:
    ClockHandler(ApplicationState& state, PostOffice<MSG::AllMessages>& post_office);

private:
    std::optional<milliseconds> OnActivation() final;

    void StartTimerTick();

    void SetTime(uint32_t utc);

    // Advance the clock by the time passed since the last update
    void Tick();

    ApplicationState& m_state;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
    os::TimerHandle m_tick_timer;

    ListenerCookie m_gps_listener;
    ApplicationState::PartialReadOnlyCache<AS::gps_utc> m_state_cache;

    uint32_t m_utc {0};
    // The time stamp of the last update, and the time since then not yet added to m_utc
    milliseconds m_last_update {0};
    milliseconds m_sub_second {0};
};
