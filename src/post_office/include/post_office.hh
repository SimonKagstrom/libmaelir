#pragma once

#include "event_notifier.hh"
#include "message_helpers.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <etl/mutex.h>
#include <etl/queue.h>
#include <etl/vector.h>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <vector>

constexpr auto kMailboxSize = 16;
constexpr auto kMaxSubscribersPerMessage = 8;

template <typename Messages>
class PostOffice;

namespace detail
{

// The stored message, independent of the message list
struct RawEnvelope
{
    uint8_t index;
    std::shared_ptr<const void> message;
};

// Shared between the subscriber and the post office, so that a Send in progress can
// outlive the Mailbox
class MailboxImpl
{
public:
    explicit MailboxImpl(IEventNotifier& notifier);

    // Drops the message if the mailbox is full
    bool Push(RawEnvelope envelope);
    std::optional<RawEnvelope> Pop();

    void Notify();
    void Detach();

private:
    etl::mutex m_mutex;
    IEventNotifier* m_notifier;
    etl::queue<RawEnvelope, kMailboxSize> m_queue;
};

} // namespace detail


// The receiving end of a subscription. Messages are kept in the order they were sent.
template <typename Messages>
class Mailbox
{
public:
    friend class PostOffice<Messages>;

    struct Envelope
    {
        template <typename T>
        bool Is() const
        {
            return raw.index == detail::IndexOfImpl<T, Messages>::Get();
        }

        // Only valid if Is<T>() is true
        template <typename T>
        std::shared_ptr<const T> As() const
        {
            assert(Is<T>());
            return std::static_pointer_cast<const T>(raw.message);
        }

        detail::RawEnvelope raw;
    };

    ~Mailbox()
    {
        m_post_office.Unsubscribe(m_impl);
        // A Send might still hold the impl, but won't notify after this
        m_impl->Detach();
    }

    Mailbox(const Mailbox&) = delete;
    Mailbox& operator=(const Mailbox&) = delete;
    Mailbox(Mailbox&&) = delete;
    Mailbox& operator=(Mailbox&&) = delete;

    // The next message, or nullopt if the mailbox is empty
    std::optional<Envelope> Pop()
    {
        if (auto raw = m_impl->Pop())
        {
            return Envelope {std::move(*raw)};
        }

        return std::nullopt;
    }

private:
    Mailbox(PostOffice<Messages>& post_office, std::shared_ptr<detail::MailboxImpl> impl)
        : m_post_office(post_office)
        , m_impl(std::move(impl))
    {
    }

    PostOffice<Messages>& m_post_office;
    std::shared_ptr<detail::MailboxImpl> m_impl;
};


// Messages is a std::tuple of all message types
template <typename Messages>
class PostOffice
{
public:
    friend class Mailbox<Messages>;

    /**
     * @brief Subscribe to a set of messages.
     *
     * @tparam MessageTypes the messages (from Messages) to receive
     * @param notifier notified when a message has been put in the mailbox
     * @return the mailbox, which unsubscribes on destruction
     */
    template <typename... MessageTypes>
    std::unique_ptr<Mailbox<Messages>> Subscribe(IEventNotifier& notifier)
    {
        static_assert(sizeof...(MessageTypes) > 0, "Subscribe requires at least one message");

        auto impl = std::make_shared<detail::MailboxImpl>(notifier);

        // Lock context
        {
            std::lock_guard lock(m_mutex);

            (AddSubscriber(IndexOf<MessageTypes>(), impl), ...);
        }

        // Private constructor, so make_unique can't be used
        return std::unique_ptr<Mailbox<Messages>>(new Mailbox<Messages>(*this, std::move(impl)));
    }

    /**
     * @brief Broadcast a message to all subscribers of it.
     *
     * Not callable from an ISR (allocates and locks).
     */
    template <typename MessageType>
    void Send(MessageType message)
    {
        constexpr auto index = IndexOf<MessageType>();

        // A single allocation, freed when the last receiver has dropped it
        std::shared_ptr<const void> p = std::make_shared<const MessageType>(std::move(message));
        std::vector<std::shared_ptr<detail::MailboxImpl>> receivers;

        // Lock context
        {
            std::lock_guard lock(m_mutex);

            // Push with the lock held, so that all mailboxes see the same order
            for (const auto& mailbox : m_subscribers[index])
            {
                mailbox->Push({index, p});
                receivers.push_back(mailbox);
            }
        }

        // ... but notify without it
        for (const auto& mailbox : receivers)
        {
            mailbox->Notify();
        }
    }

private:
    template <typename T>
    static consteval uint8_t IndexOf()
    {
        return detail::IndexOfImpl<T, Messages>::Get();
    }

    void AddSubscriber(uint8_t index, const std::shared_ptr<detail::MailboxImpl>& impl)
    {
        assert(m_subscribers[index].size() < kMaxSubscribersPerMessage);

        m_subscribers[index].push_back(impl);
    }

    void Unsubscribe(const std::shared_ptr<detail::MailboxImpl>& impl)
    {
        std::lock_guard lock(m_mutex);

        for (auto& subscribers : m_subscribers)
        {
            std::erase(subscribers, impl);
        }
    }

    etl::mutex m_mutex;
    std::array<std::vector<std::shared_ptr<detail::MailboxImpl>>, std::tuple_size_v<Messages>>
        m_subscribers;
};
