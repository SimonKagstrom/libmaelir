#include "application_state.hh"

#include <mutex>

constexpr auto kInvalidListener = 0;

static os::binary_semaphore g_dummy_sem {0};


ApplicationState::ReadOnly::ReadOnly(ApplicationState& parent)
    : m_parent(parent)
{
}

ApplicationState::ApplicationState()
{
    m_global_state.SetupDefaultValues();
    m_listener_semaphores.push_back(&g_dummy_sem);
}

std::unique_ptr<ListenerCookie>
ApplicationState::DoAttachListener(const ParameterBitset& interested,
                                   os::binary_semaphore& semaphore)
{
    std::lock_guard lock(m_mutex);

    if (m_reclaimed_listener_indices.empty() &&
        m_listener_semaphores.size() > std::numeric_limits<uint8_t>::max())
    {
        return nullptr;
    }

    auto index = static_cast<uint8_t>(m_listener_semaphores.size());

    if (m_reclaimed_listener_indices.empty())
    {
        m_listener_semaphores.push_back(&semaphore);
    }
    else
    {
        index = m_reclaimed_listener_indices.back();
        m_reclaimed_listener_indices.pop_back();
        m_listener_semaphores[index] = &semaphore;
    }

    for (auto param_index = interested.find_first(true); param_index != interested.npos;
         param_index = interested.find_next(true, param_index + 1))
    {
        m_listeners[param_index].push_back(index);
    }


    return std::make_unique<ListenerCookie>([this, index, interested]() {
        std::lock_guard lock(m_mutex);

        m_listener_semaphores[index] = &g_dummy_sem;
        for (auto param_index = interested.find_first(true); param_index != interested.npos;
             param_index = interested.find_next(true, param_index + 1))
        {
            auto& listeners = m_listeners[param_index];
            listeners.erase(std::remove(listeners.begin(), listeners.end(), index),
                            listeners.end());
        }
        m_reclaimed_listener_indices.push_back(index);
    });
}

void
ApplicationState::NotifyChange(unsigned index)
{
    if (index >= m_listeners.size())
    {
        return;
    }

    for (auto listener : m_listeners[index])
    {
        m_listener_semaphores[listener]->release();
    }
}


ApplicationState::ReadWrite
ApplicationState::CheckoutReadWrite()
{
    return ApplicationState::ReadWrite(*this);
}

ApplicationState::ReadOnly
ApplicationState::CheckoutReadonly()
{
    return ApplicationState::ReadOnly(*this);
}
