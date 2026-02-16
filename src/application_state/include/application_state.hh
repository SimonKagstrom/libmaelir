#pragma once

#include "generated_application_state.hh"
#include "listener_cookie.hh"
#include "semaphore.hh"

#include <array>
#include <atomic>
#include <etl/bitset.h>
#include <etl/mutex.h>
#include <etl/vector.h>
#include <string_view>

constexpr auto kInvalidIconHash = 0;

using ParameterBitset = etl::bitset<AS::kLastIndex + 1, uint32_t>;

namespace AS::storage
{

template <class... T>
struct partial_state : public T...
{
    template <typename S>
    auto& GetRef()
    {
        return static_cast<S&>(*this).template GetRef<S>();
    }

    // In the future, maybe provide a IndexOfPartial here, for local indicies (bitmask)
};

} // namespace AS::storage

class ApplicationState
{
public:
    class ReadOnly
    {
    public:
        friend class ApplicationState;

        template <typename T>
        auto Get()
        {
            return m_parent.Get<T>();
        }

    protected:
        explicit ReadOnly(ApplicationState& parent);

        ApplicationState& m_parent;
    };

    class ReadWrite : public ReadOnly
    {
    public:
        friend class ApplicationState;

        template <typename T>
        void Set(const auto& value)
        {
            std::lock_guard lock(m_parent.m_mutex);
            m_parent.SetUnlocked<T>(value);
        }

    private:
        using ReadOnly::ReadOnly;
    };


    template <class... T>
    class PartialSnapshot
    {
    public:
        friend class ApplicationState;

        ~PartialSnapshot()
        {
            if (m_changed.none())
            {
                return;
            }

            std::lock_guard lock(m_parent.m_mutex);

            (void)std::initializer_list<int> {
                (m_changed.test(AS::IndexOf<T>()) ? (m_parent.SetUnlocked<T>(Get<T>()), 0) : 0)...};
        }

        template <typename S>
        auto Get()
        {
            // Require that S is in T...
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            // Never a shared_ptr
            return m_state.template GetRef<S>();
        }

        template <typename S>
        void Set(const auto& value)
        {
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            m_state.template GetRef<S>() = value;
            m_changed.set(AS::IndexOf<S>());
        }

    private:
        explicit PartialSnapshot(ApplicationState& parent)
            : m_parent(parent)
        {
            (void)std::initializer_list<int> {
                (m_state.template GetRef<T>() = m_parent.GetValue<T>(), 0)...};
        }

        ApplicationState& m_parent;
        AS::storage::partial_state<T...> m_state;

        ParameterBitset m_changed;
    };


    template <class... T>
    class QueuedWriter
    {
    public:
        friend class ApplicationState;

        ~QueuedWriter()
        {
            if (m_changed.none())
            {
                return;
            }

            std::lock_guard lock(m_parent.m_mutex);

            (void)std::initializer_list<int> {
                (m_changed.test(AS::IndexOf<T>()) ? (m_parent.SetUnlocked<T>(Get<T>()), 0) : 0)...};
        }

        template <typename S>
        void Set(const auto& value)
        {
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            m_state.template GetRef<S>() = value;
            m_changed.set(AS::IndexOf<S>());
        }

    private:
        explicit QueuedWriter(ApplicationState& parent)
            : m_parent(parent)
        {
            // No initialization, these are write-only
        }

        template <typename S>
        auto Get()
        {
            // These are write-only, so reads are not allowed
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            return m_state.template GetRef<S>();
        }

        ApplicationState& m_parent;
        AS::storage::partial_state<T...> m_state;

        ParameterBitset m_changed;
    };


    ApplicationState();


    template <class... T>
    std::unique_ptr<ListenerCookie> AttachListener(os::binary_semaphore& semaphore)
    {
        ParameterBitset interested;

        (void)std::initializer_list<int> {(interested.set<AS::IndexOf<T>()>(), 0)...};

        return DoAttachListener(interested, semaphore);
    }

    // Checkout a local copy of the global state. Rewritten when the unique ptr is released
    ReadWrite CheckoutReadWrite();

    ReadOnly CheckoutReadonly();


    template <class... T>
    auto CheckoutPartialSnapshot()
    {
        return PartialSnapshot<T...>(*this);
    }

    template <class... T>
    auto CheckoutQueuedWriter()
    {
        return QueuedWriter<T...>(*this);
    }

private:
    class ListenerImpl;
    class StateImpl;

    template <typename T>
    auto Get()
    {
        if constexpr (T::IsAtomic())
        {
            return m_global_state.GetRef<T>();
        }
        else
        {
            auto ptr = m_global_state.GetRef<T>();

            using element_type = typename decltype(ptr)::element_type;
            return std::make_shared<const element_type>(*ptr);
        }
    }

    template <typename T>
    auto GetValue()
    {
        if constexpr (T::IsAtomic())
        {
            return m_global_state.GetRef<T>();
        }
        else
        {
            return *m_global_state.GetRef<T>();
        }
    }


    template <typename T>
    void SetUnlocked(const auto& value)
    {
        if (value == GetValue<T>())
        {
            return;
        }


        if constexpr (T::IsAtomic())
        {
            m_global_state.GetRef<T>() = value;
        }
        else
        {
            auto& ref = m_global_state.GetRef<T>();

            m_global_state.GetRef<T>() = std::make_shared<std::decay_t<decltype(*ref)>>(value);
        }

        NotifyChange(AS::IndexOf<T>());
    }

    std::unique_ptr<ListenerCookie> DoAttachListener(const ParameterBitset& interested,
                                                     os::binary_semaphore& semaphore);
    void NotifyChange(unsigned index);

    AS::storage::state m_global_state;

    etl::mutex m_mutex;

    std::array<std::vector<uint8_t>, AS::kLastIndex + 1> m_listeners;
    std::vector<os::binary_semaphore*> m_listener_semaphores;
    std::vector<uint8_t> m_reclaimed_listener_indices;
};
