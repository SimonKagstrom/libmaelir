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

    // Might be used in the future, if the number of global indicies are > 32
    template <typename S>
    static consteval auto PartialIndexOf()
    {
        static_assert((std::is_same_v<S, T> || ...), "Type is not part of this partial_state");

        constexpr std::array<bool, sizeof...(T)> matches {std::is_same_v<S, T>...};
        for (size_t index = 0; index < matches.size(); ++index)
        {
            if (matches[index])
            {
                return index;
            }
        }

        return size_t {0};
    }

    template <typename S>
    static consteval auto PartialToGlobalIndex(auto partial_index)
    {
        static_assert((std::is_same_v<S, T> || ...), "Type is not part of this partial_state");

        constexpr std::array<unsigned, sizeof...(T)> global_indices {AS::IndexOf<T>()...};
        return global_indices[partial_index];
    }
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
            m_parent.SetNoLock<T>(value);
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
                (m_changed.test(AS::IndexOf<T>()) ? (m_parent.SetNoLock<T>(Get<T>()), 0) : 0)...};
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
            // Hold the lock since the GetValue might refer to a shared_ptr
            std::lock_guard lock(m_parent.m_mutex);

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
                (m_changed.test(AS::IndexOf<T>()) ? (m_parent.SetNoLock<T>(Get<T>()), 0) : 0)...};
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

    /**
     * @brief Checkout a reader-only of the entire global state.
     *
     * Reads return the global state, so can change at any time.
     *
     * @return A reader
     */
    ReadOnly CheckoutReadonly();

    /**
     * @brief Checkout a reader-writer of the entire global state.
     *
     * Reads return the global state, so can change at any time. Writes are
     * done immediately.
     *
     * @return A reader-writer.
     */
    ReadWrite CheckoutReadWrite();


    /**
     * @brief Checkout a snapshot of a subset of the global state
     *
     * The snapshot is cached, so global writes do not affect it. Writes are
     * written back on destruction. Reads/writes can only be done of the subset.
     *
     * @tparam T a list of members of the global state
     * @return the partial snapshot
     */
    template <class... T>
    auto CheckoutPartialSnapshot()
    {
        return PartialSnapshot<T...>(*this);
    }

    /**
     * @brief Checkout a queued writer of a subset of the global state
     *
     * This is write-only, and can only write the subset. Writes are written back
     * on destruction.
     *
     * @tparam T a list of members of the global state
     * @return the queued writer
     */

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
            /* On return, the shared_ptr to const ensures the value stays alive even if
             * it's written, until the reader is done with it.
             */
            using pointer_type = std::remove_reference_t<decltype(m_global_state.GetRef<T>())>;
            using element_type = typename pointer_type::element_type;

            std::shared_ptr<const element_type> out = m_global_state.GetRef<T>();

            return out;
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
    void SetNoLock(const auto& value)
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
