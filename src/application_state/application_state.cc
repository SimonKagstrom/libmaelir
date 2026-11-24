#include "application_state.hh"

#include "debug_assert.hh"
#include "semaphore.hh"

#include <mutex>

static os::binary_semaphore g_dummy_sem {0};

ApplicationState::ApplicationState()
{
    m_global_state.SetupDefaultValues();
}

ListenerCookie
ApplicationState::DoAttachListener(const ParameterBitset& interested, IEventNotifier& notifier)
{
    std::lock_guard lock(m_mutex);

    const auto full = m_reclaimed_listener_indices.empty() &&
                      m_listener_notifiers_by_index.size() >= kMaxApplicationStateListeners;
    // The listener would never be notified. Increase kMaxApplicationStateListeners if this triggers
    debug_assert(!full);
    if (full)
    {
        return {};
    }

    auto index = static_cast<uint8_t>(m_listener_notifiers_by_index.size());

    if (m_reclaimed_listener_indices.empty())
    {
        m_listener_notifiers_by_index.push_back(&notifier);
    }
    else
    {
        index = m_reclaimed_listener_indices.back();
        m_reclaimed_listener_indices.pop_back();
        m_listener_notifiers_by_index[index] = &notifier;
    }

    for (auto param_index = interested.find_first(true); param_index != interested.npos;
         param_index = interested.find_next(true, param_index + 1))
    {
        m_listeners[param_index].set(index);
    }


    return ListenerCookie([this, index]() {
        std::lock_guard lock(m_mutex);

        m_listener_notifiers_by_index[index] = &g_dummy_sem;
        for (auto& listeners : m_listeners)
        {
            listeners.reset(index);
        }
        m_reclaimed_listener_indices.push_back(index);
    });
}

void
ApplicationState::NotifyChange(unsigned parameter_index)
{
    if (parameter_index >= m_listeners.size())
    {
        return;
    }

    NotifyListeners(m_listeners[parameter_index]);
}

void
ApplicationState::NotifyMultipleChanges(const ParameterBitset& changed)
{
    ListenerBitset listeners;

    // Collect the listeners to notify (avoid notifying the same listener multiple times)
    for (auto param_index = changed.find_first(true); param_index != changed.npos;
         param_index = changed.find_next(true, param_index + 1))
    {
        listeners |= m_listeners[param_index];
    }

    NotifyListeners(listeners);
}

void
ApplicationState::NotifyListeners(const ListenerBitset& listeners)
{
    for (auto listener_index = listeners.find_first(true); listener_index != listeners.npos;
         listener_index = listeners.find_next(true, listener_index + 1))
    {
        m_listener_notifiers_by_index[listener_index]->Notify();
    }
}
