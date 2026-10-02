#include "post_office.hh"

using namespace detail;

MailboxImpl::MailboxImpl(IEventNotifier& notifier)
    : m_notifier(&notifier)
{
}

bool
MailboxImpl::Push(RawEnvelope envelope)
{
    std::lock_guard lock(m_mutex);

    if (m_queue.full())
    {
        return false;
    }

    m_queue.push(std::move(envelope));
    return true;
}

std::optional<RawEnvelope>
MailboxImpl::Pop()
{
    std::lock_guard lock(m_mutex);

    if (m_queue.empty())
    {
        return std::nullopt;
    }

    auto out = std::move(m_queue.front());
    m_queue.pop();

    return out;
}

void
MailboxImpl::Notify()
{
    // Hold the lock, so that Detach can't complete (and the notifier go away) meanwhile
    std::lock_guard lock(m_mutex);

    if (m_notifier)
    {
        m_notifier->Notify();
    }
}

void
MailboxImpl::Detach()
{
    std::lock_guard lock(m_mutex);

    m_notifier = nullptr;
}

