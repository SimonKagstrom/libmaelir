#include "post_office.hh"

using namespace detail;

namespace
{

// Enough for the common case, so that a Push (with the post office lock held) seldom allocates
constexpr auto kInitialMailboxCapacity = 4;
// Grown beyond this by a burst of messages, the storage is given back
constexpr auto kShrinkMailboxCapacity = 16;

} // namespace

MailboxImpl::MailboxImpl(IEventNotifier& notifier)
    : m_notifier(&notifier)
{
    m_incoming.reserve(kInitialMailboxCapacity);
    m_retrieved.reserve(kInitialMailboxCapacity);
}

void
MailboxImpl::Push(RawEnvelope envelope)
{
    std::lock_guard lock(m_mutex);

    m_incoming.push_back(std::move(envelope));
}

const std::vector<RawEnvelope>&
MailboxImpl::RetrieveAll()
{
    // Otherwise Recycle hasn't been called, or a handler collects from its own mailbox
    assert(m_retrieved.empty());

    std::lock_guard lock(m_mutex);

    m_incoming.swap(m_retrieved);

    return m_retrieved;
}

void
MailboxImpl::Recycle()
{
    // Without the lock, since the last references to the messages might be dropped here

    if (m_retrieved.capacity() > kShrinkMailboxCapacity)
    {
        // Not shrink_to_fit, which is non-binding
        std::vector<RawEnvelope> fresh;

        fresh.reserve(kInitialMailboxCapacity);
        m_retrieved.swap(fresh);
    }
    else
    {
        m_retrieved.clear();
    }
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
