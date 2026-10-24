#include "clock_handler.hh"

#include "time.hh"

ClockHandler::ClockHandler(ApplicationState& state, PostOffice<MSG::AllMessages>& post_office)
    : m_state(state)
    , m_mailbox(post_office.Subscribe<MSG::network_time>(*this))
{
}

std::optional<milliseconds>
ClockHandler::OnActivation()
{
    m_mailbox->Collect().On<MSG::network_time>([this](auto time) {
        if (time->timezone_minutes)
        {
            m_state.Set<AS::timezone>(*time->timezone_minutes);
        }
        SetTime(time->utc);
    });

    return std::nullopt;
}

void
ClockHandler::SetTime(uint32_t utc)
{
    m_utc = utc;
    m_sub_second = 0ms;
    m_last_update = os::GetTimeStamp();
    m_state.Set<AS::clock>(m_utc);

    // Not exact (the job pool wakes up irregularly), but good enough for showing minutes
    m_tick_timer = StartTimer(1s, [this]() {
        Tick();
        return 1s;
    });
}

void
ClockHandler::Tick()
{
    // Since the time stamp is unsigned, this is correct also when it wraps (after ~49 days)
    const auto now = os::GetTimeStamp();
    m_sub_second += now - m_last_update;
    m_last_update = now;

    // Count whole seconds from the actual time passed, and keep the rest, so that late
    // wakeups don't cause drift
    const auto whole_seconds = std::chrono::duration_cast<seconds>(m_sub_second);
    m_sub_second -= whole_seconds;
    m_utc += whole_seconds.count();
    m_state.Set<AS::clock>(m_utc);
}
