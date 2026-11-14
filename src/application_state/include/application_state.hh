#pragma once

#include "event_notifier.hh"
#include "generated_application_state.hh"
#include "listener_cookie.hh"

#include <array>
#include <atomic>
#include <etl/bitset.h>
#include <etl/delegate.h>
#include <etl/mutex.h>
#include <etl/vector.h>
#include <memory>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <utility>

using ParameterBitset = etl::bitset<AS::kLastIndex + 1, uint32_t>;

// Fit in an uint32_t
constexpr auto kMaxApplicationStateListeners = 32;

namespace AS::storage
{

/*
 * We'd like to use std::atomic<std::shared_ptr<T>>, but libc++ (clang) doesn't support that.
 * The free functions are deprecated since C++20, so silence that warning for these only.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
template <typename T>
std::shared_ptr<const T>
LoadShared(const std::shared_ptr<T>& pointer)
{
    return std::atomic_load_explicit(&pointer, std::memory_order_acquire);
}

template <typename T>
void
StoreShared(std::shared_ptr<T>& pointer, std::shared_ptr<T> value)
{
    std::atomic_store_explicit(&pointer, std::move(value), std::memory_order_release);
}
#pragma GCC diagnostic pop

template <class... T>
struct partial_state : public T...
{
    template <typename S>
    auto& GetRef()
    {
        return static_cast<S&>(*this).template GetRef<S>();
    }

    template <typename S>
    const auto& GetConstRef() const
    {
        return static_cast<const S&>(*this).template GetConstRef<S>();
    }

    static consteval auto PartialCount()
    {
        return sizeof...(T);
    }

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

    using ParameterBitset = etl::bitset<sizeof...(T), uint32_t>;
};

// The type of the value of a parameter, e.g., GpsData for AS::position
template <typename S>
using value_type_of = std::remove_cvref_t<decltype(std::declval<S&>().template GetRef<S>())>;

/*
 * How a read-only cache holds a parameter: Atomics by value, and others as a pointer to the
 * published value. A published value is never modified, and a changed value is always
 * published with a new pointer. Pulling therefore only copies the pointer, and an unchanged
 * pointer means an unchanged value.
 */
template <typename S>
using cached_type_of = std::conditional_t<S::IsAtomic(),
                                          value_type_of<S>,
                                          std::shared_ptr<const value_type_of<S>>>;

template <typename S>
struct cache_slot
{
    cached_type_of<S> value {};
};

template <class... T>
struct cached_state : public cache_slot<T>...
{
    template <typename S>
    auto& Slot()
    {
        return static_cast<cache_slot<S>&>(*this).value;
    }

    template <typename S>
    const auto& Slot() const
    {
        return static_cast<const cache_slot<S>&>(*this).value;
    }
};

} // namespace AS::storage

class ApplicationState
{
public:
    template <class... T>
    class PartialReadOnlyCache
    {
    public:
        class Checkout
        {
        public:
            friend class PartialReadOnlyCache;

            Checkout() = delete;
            Checkout(const Checkout&) = delete;
            Checkout& operator=(const Checkout&) = delete;
            Checkout(Checkout&&) = delete;
            Checkout& operator=(Checkout&&) = delete;

            template <typename S>
            bool IsChanged() const
            {
                return m_changed.test(PartialState::template PartialIndexOf<S>());
            }

            template <typename S>
            const Checkout& OnChanged(const auto& callback) const
            {
                if (IsChanged<S>())
                {
                    callback();
                }

                return *this;
            }

            template <typename S>
            const Checkout& OnNewValue(const auto& callback) const
            {
                if (IsChanged<S>())
                {
                    callback(ValueAt<S>(m_state_index));
                }

                return *this;
            }

            template <typename S>
            const Checkout& OnChangedValue(const auto& callback) const
            {
                if (IsChanged<S>())
                {
                    callback(ValueAt<S>(!m_state_index), ValueAt<S>(m_state_index));
                }

                return *this;
            }

            /**
             * @brief Return the value of the local storage
             *
             * Atomic parameters are returned by value, and others by reference to avoid
             * copying. The reference is valid until the second Pull() after it was taken,
             * i.e., after the next Pull() it refers to the previous value.
             */
            template <typename S>
            decltype(auto) Get() const
            {
                if constexpr (S::IsAtomic())
                {
                    return AS::storage::value_type_of<S>(ValueAt<S>(m_state_index));
                }
                else
                {
                    return ValueAt<S>(m_state_index);
                }
            }

        private:
            using PartialState = AS::storage::partial_state<T...>;
            using CachedState = AS::storage::cached_state<T...>;

            explicit Checkout(ApplicationState& parent)
                : m_parent(parent)
            {
                m_state[0] = Load();
                m_state[1] = m_state[0];
            }

            // Read the global state. Only pointers are copied for non-atomics, so the
            // lock (for a consistent view of multiple parameters) is held only briefly
            CachedState Load() const
            {
                CachedState out;
                auto lock = GetLock();

                ((out.template Slot<T>() = m_parent.template Get<T>()), ...);

                return out;
            }

            template <typename S>
            void UpdateChanges(uint8_t cur, uint8_t next)
            {
                const auto& old_value = m_state[cur].template Slot<S>();
                const auto& new_value = m_state[next].template Slot<S>();

                bool changed;
                if constexpr (S::IsAtomic())
                {
                    changed = old_value != new_value;
                }
                else
                {
                    // Only compare the values if a new value has been published. They can
                    // still be equal, e.g., if changed back and forth between two pulls
                    changed = old_value != new_value && *old_value != *new_value;
                }

                if (changed)
                {
                    m_changed.set(PartialState::template PartialIndexOf<S>());
                }
            }

            template <typename S>
            const auto& ValueAt(uint8_t index) const
            {
                // Require that S is in T...
                static_assert(std::disjunction_v<std::is_same<S, T>...>);

                const auto& slot = m_state[index].template Slot<S>();
                if constexpr (S::IsAtomic())
                {
                    return slot;
                }
                else
                {
                    return *slot;
                }
            }

            std::unique_lock<etl::mutex> GetLock() const
            {
                if constexpr (sizeof...(T) == 1)
                {
                    // No mutex needed here
                    return std::unique_lock<etl::mutex>();
                }
                return std::unique_lock<etl::mutex>(m_parent.m_mutex);
            }

            ApplicationState& m_parent;

            std::array<CachedState, 2> m_state;
            uint8_t m_state_index {0};
            PartialState::ParameterBitset m_changed;
        };

        friend class ApplicationState;

        PartialReadOnlyCache(const PartialReadOnlyCache&) = delete;
        PartialReadOnlyCache& operator=(const PartialReadOnlyCache&) = delete;
        PartialReadOnlyCache(PartialReadOnlyCache&&) = delete;
        PartialReadOnlyCache& operator=(PartialReadOnlyCache&&) = delete;

        explicit PartialReadOnlyCache(ApplicationState& parent)
            : m_checkout(parent)
        {
        }

        const Checkout& Pull()
        {
            m_checkout.m_changed.reset();
            const uint8_t cur = m_checkout.m_state_index;
            const uint8_t next = !cur;

            // Assigned outside the lock, since replacing the old pointers can free values
            m_checkout.m_state[next] = m_checkout.Load();

            (m_checkout.template UpdateChanges<T>(cur, next), ...);
            m_checkout.m_state_index = next;

            return m_checkout;
        }


        /// Return the value of the local storage, see Checkout::Get()
        template <typename S>
        decltype(auto) Get() const
        {
            return m_checkout.template Get<S>();
        }

    private:
        Checkout m_checkout;
    };


    template <class... T>
    class PartialSnapshot
    {
    public:
        friend class ApplicationState;

        PartialSnapshot(const PartialSnapshot&) = delete;
        PartialSnapshot& operator=(const PartialSnapshot&) = delete;
        PartialSnapshot(PartialSnapshot&&) = delete;
        PartialSnapshot& operator=(PartialSnapshot&&) = delete;


        ~PartialSnapshot()
        {
            if (m_changed.none())
            {
                return;
            }

            ParameterBitset global_changed;

            std::lock_guard lock(m_parent.m_mutex);

            (WriteBack<T>(global_changed), ...);

            m_parent.NotifyMultipleChanges(global_changed);
        }

        /// Return a reference to the local value
        template <typename S>
        auto& GetWritableReference()
        {
            // Assume it being changed when a reference is used (the actual check will be during writeback)
            m_changed.set(PartialState::template PartialIndexOf<S>());

            return GetReference<S>();
        }

        /// Return the value of the local storage
        template <typename S>
        auto Get()
        {
            return GetReference<S>();
        }


        /// Write a value, which is converted to the type of the parameter by the caller
        template <typename S>
        void Set(AS::storage::value_type_of<S> value)
        {
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            m_state.template GetRef<S>() = std::move(value);
            m_changed.set(PartialState::template PartialIndexOf<S>());
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

        template <typename S>
        auto& GetReference()
        {
            // Require that S is in T...
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            return m_state.template GetRef<S>();
        }

        // Called with the lock held. The value is moved, since the snapshot is being destroyed
        template <typename S>
        void WriteBack(ParameterBitset& global_changed)
        {
            if (m_changed.test(PartialState::template PartialIndexOf<S>()))
            {
                m_parent.template SetNoLockCollectChanged<S>(std::move(GetReference<S>()),
                                                             global_changed);
            }
        }

        using PartialState = AS::storage::partial_state<T...>;

        ApplicationState& m_parent;
        PartialState m_state;

        PartialState::ParameterBitset m_changed;
    };


    template <class... T>
    class QueuedWriter
    {
    public:
        friend class ApplicationState;

        QueuedWriter(const QueuedWriter&) = delete;
        QueuedWriter& operator=(const QueuedWriter&) = delete;
        QueuedWriter(QueuedWriter&&) = delete;
        QueuedWriter& operator=(QueuedWriter&&) = delete;

        ~QueuedWriter()
        {
            if (m_changed.none())
            {
                return;
            }
            ParameterBitset global_changed;

            std::lock_guard lock(m_parent.m_mutex);

            (WriteBack<T>(global_changed), ...);

            m_parent.NotifyMultipleChanges(global_changed);
        }

        /// Write a value, which is converted to the type of the parameter by the caller
        template <typename S>
        void Set(AS::storage::value_type_of<S> value)
        {
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            m_state.template GetRef<S>() = std::move(value);
            m_changed.set(PartialState::template PartialIndexOf<S>());
        }

    private:
        explicit QueuedWriter(ApplicationState& parent)
            : m_parent(parent)
        {
            // No initialization, these are write-only
        }

        // Called with the lock held. The value is moved, since the writer is being destroyed
        template <typename S>
        void WriteBack(ParameterBitset& global_changed)
        {
            if (m_changed.test(PartialState::template PartialIndexOf<S>()))
            {
                m_parent.template SetNoLockCollectChanged<S>(
                    std::move(m_state.template GetRef<S>()), global_changed);
            }
        }

        using PartialState = AS::storage::partial_state<T...>;

        ApplicationState& m_parent;
        PartialState m_state;

        PartialState::ParameterBitset m_changed;
    };


    ApplicationState();


    template <class... T>
    ListenerCookie AttachListener(IEventNotifier& notifier)
    {
        static_assert((sizeof...(T) > 0) && "AttachListener requires at least one parameter");
        ParameterBitset interested;

        (void)std::initializer_list<int> {(interested.set<AS::IndexOf<T>()>(), 0)...};

        return DoAttachListener(interested, notifier);
    }

    /**
     * @brief Checkout a snapshot of a subset of the global state
     *
     * The snapshot is cached, so global writes do not affect it. Writes are
     * written back on destruction. Reads/writes can only be done of the subset.
     *
     * Note that the write-back is of whole values, without checking for other writes
     * since the checkout. A read-modify-write in a snapshot can therefore overwrite (lose)
     * a concurrent write from another thread, e.g., two threads changing different fields
     * of the same struct. This is fine with a single writer thread per parameter, which is
     * the normal case.
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

    // Read-only gets are allowed, so this is public
    template <typename T>
    auto Get()
    {
        if constexpr (T::IsAtomic())
        {
            return m_global_state.GetRef<T>().load();
        }
        else
        {
            /* On return, the shared_ptr to const ensures the value stays alive even if
             * it's written, until the reader is done with it.
             */
            return AS::storage::LoadShared(m_global_state.GetRef<T>());
        }
    }

    /**
     * @brief Write a single member of the global state
     *
     * The write is done (and listeners notified) immediately. Use a queued writer
     * to update multiple members together.
     *
     * The value is converted to the type of the parameter by the caller, i.e., before
     * comparing it with the current value. An rvalue is moved into the global state.
     */
    template <typename T>
    void Set(AS::storage::value_type_of<T> value)
    {
        std::lock_guard lock(m_mutex);
        SetNoLock<T>(std::move(value));
    }

private:
    class ListenerImpl;
    class StateImpl;

    template <typename T>
    auto GetValue()
    {
        if constexpr (T::IsAtomic())
        {
            return m_global_state.GetRef<T>().load();
        }
        else
        {
            return *AS::storage::LoadShared(m_global_state.GetRef<T>());
        }
    }

    // Called with the lock held. Compares without copying the current value
    template <typename T>
    bool IsCurrentValue(const auto& value)
    {
        if constexpr (T::IsAtomic())
        {
            return value == m_global_state.GetRef<T>().load();
        }
        else
        {
            return value == *AS::storage::LoadShared(m_global_state.GetRef<T>());
        }
    }

    template <typename T>
    void DoSetValue(auto&& value)
    {
        if constexpr (T::IsAtomic())
        {
            m_global_state.GetRef<T>() = value;
        }
        else
        {
            using pointer_type = std::remove_reference_t<decltype(m_global_state.GetRef<T>())>;
            using element_type = typename pointer_type::element_type;

            AS::storage::StoreShared(
                m_global_state.GetRef<T>(),
                std::make_shared<element_type>(std::forward<decltype(value)>(value)));
        }
    }

    template <typename T>
    void SetNoLock(auto&& value)
    {
        if (IsCurrentValue<T>(value))
        {
            return;
        }

        DoSetValue<T>(std::forward<decltype(value)>(value));

        NotifyChange(AS::IndexOf<T>());
    }

    template <typename T>
    void SetNoLockCollectChanged(auto&& value, ParameterBitset& changed)
    {
        if (IsCurrentValue<T>(value))
        {
            return;
        }

        DoSetValue<T>(std::forward<decltype(value)>(value));

        changed.set(AS::IndexOf<T>());
    }

    ListenerCookie DoAttachListener(const ParameterBitset& interested,
                                                     IEventNotifier& notifier);
    void NotifyChange(unsigned index);

    void NotifyMultipleChanges(const ParameterBitset& changed);

    AS::storage::state m_global_state;

    etl::mutex m_mutex;

    std::array<std::vector<uint8_t>, AS::kLastIndex + 1> m_listeners;
    std::vector<IEventNotifier*> m_listener_notifiers_by_index;
    std::vector<uint8_t> m_reclaimed_listener_indices;
};
