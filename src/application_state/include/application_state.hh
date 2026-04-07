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
#include <string_view>

using ParameterBitset = etl::bitset<AS::kLastIndex + 1, uint32_t>;

// Fit in an uint32_t
constexpr auto kMaxApplicationStateListeners = 32;

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

    template <typename S>
    const auto& GetConstRef() const
    {
        return static_cast<const S&>(*this).template GetConstRef<S>();
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

        ReadWrite(const ReadWrite&) = delete;
        ReadWrite& operator=(const ReadWrite&) = delete;
        ReadWrite(ReadWrite&&) = delete;
        ReadWrite& operator=(ReadWrite&&) = delete;


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
                return m_changed.test(AS::IndexOf<S>());
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
                    callback(Get<S>());
                }

                return *this;
            }

            template <typename S>
            const Checkout& OnChangedValue(const auto& callback) const
            {
                if (IsChanged<S>())
                {
                    callback(GetReference<S>(!m_state_index), GetReference<S>(m_state_index));
                }

                return *this;
            }

            /// Return the value of the local storage
            template <typename S>
            auto Get() const
            {
                return GetReference<S>(m_state_index);
            }

        private:
            explicit Checkout(ApplicationState& parent)
                : m_parent(parent)
            {
                // Lock context
                {
                    // Hold the lock since the GetValue might refer to a shared_ptr
                    auto lock = GetLock();

                    (void)std::initializer_list<int> {
                        (m_state[0].template GetRef<T>() = parent.GetValue<T>(), 0)...};
                }
                m_state[1] = m_state[0];
            }

            template <typename S>
            void UpdateChanges(uint8_t cur, uint8_t next)
            {
                if (GetReference<S>(cur) != GetReference<S>(next))
                {
                    m_changed.set(AS::IndexOf<S>());
                }
            }

            template <typename S>
            const auto& GetReference(uint8_t index) const
            {
                // Require that S is in T...
                static_assert(std::disjunction_v<std::is_same<S, T>...>);

                return m_state[index].template GetConstRef<S>();
            }

            std::unique_lock<std::mutex> GetLock() const
            {
                if constexpr (sizeof...(T) == 1)
                {
                    // No mutex needed here
                    return std::unique_lock<std::mutex>();
                }
                return std::unique_lock<std::mutex>(m_parent.m_mutex);
            }

            ApplicationState& m_parent;

            // TODO:  Use local index
            std::array<AS::storage::partial_state<T...>, 2> m_state;
            uint8_t m_state_index {0};
            ParameterBitset m_changed;
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

            // Only do the sync with the lock held, the rest is local
            {
                auto mutex = m_checkout.GetLock();

                (void)std::initializer_list<int> {(m_checkout.m_state[next].template GetRef<T>() =
                                                       m_checkout.m_parent.template GetValue<T>(),
                                                   0)...};
            }

            (void)std::initializer_list<int> {
                (m_checkout.template UpdateChanges<T>(cur, next), 0)...};
            m_checkout.m_state_index = next;

            return m_checkout;
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

            (void)std::initializer_list<int> {
                (m_changed.test(AS::IndexOf<T>())
                     ? (m_parent.SetNoLockCollectChanged<T>(Get<T>(), global_changed), 0)
                     : 0)...};

            m_parent.NotifyMultipleChanges(global_changed);
        }

        /// Return a reference to the local value
        template <typename S>
        auto& GetWritableReference()
        {
            // Assume it being changed when a reference is used (the actual check will be during writeback)
            m_changed.set(AS::IndexOf<S>());

            return GetReference<S>();
        }

        /// Return the value of the local storage
        template <typename S>
        auto Get()
        {
            return GetReference<S>();
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

        template <typename S>
        auto& GetReference()
        {
            // Require that S is in T...
            static_assert(std::disjunction_v<std::is_same<S, T>...>);

            return m_state.template GetRef<S>();
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

            (void)std::initializer_list<int> {
                (m_changed.test(AS::IndexOf<T>())
                     ? (m_parent.SetNoLockCollectChanged<T>(Get<T>(), global_changed), 0)
                     : 0)...};

            m_parent.NotifyMultipleChanges(global_changed);
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
    std::unique_ptr<ListenerCookie> AttachListener(IEventNotifier& notifier)
    {
        static_assert((sizeof...(T) > 0) && "AttachListener requires at least one parameter");
        ParameterBitset interested;

        (void)std::initializer_list<int> {(interested.set<AS::IndexOf<T>()>(), 0)...};

        return DoAttachListener(interested, notifier);
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
            return m_global_state.GetRef<T>().load();
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
            return m_global_state.GetRef<T>().load();
        }
        else
        {
            return *m_global_state.GetRef<T>();
        }
    }


    template <typename T>
    void DoSetValue(const auto& value)
    {
        if constexpr (T::IsAtomic())
        {
            m_global_state.GetRef<T>() = value;
        }
        else
        {
            auto& ref = m_global_state.GetRef<T>();

            m_global_state.GetRef<T>() = std::make_shared<std::decay_t<decltype(*ref)>>(value);
        }
    }

    template <typename T>
    void SetNoLock(const auto& value)
    {
        if (value == GetValue<T>())
        {
            return;
        }

        DoSetValue<T>(value);

        NotifyChange(AS::IndexOf<T>());
    }

    template <typename T>
    void SetNoLockCollectChanged(const auto& value, ParameterBitset& changed)
    {
        if (value == GetValue<T>())
        {
            return;
        }

        DoSetValue<T>(value);

        changed.set(AS::IndexOf<T>());
    }

    std::unique_ptr<ListenerCookie> DoAttachListener(const ParameterBitset& interested,
                                                     IEventNotifier& notifier);
    void NotifyChange(unsigned index);

    void NotifyMultipleChanges(const ParameterBitset& changed);

    AS::storage::state m_global_state;

    etl::mutex m_mutex;

    std::array<std::vector<uint8_t>, AS::kLastIndex + 1> m_listeners;
    std::vector<IEventNotifier*> m_listener_notifiers_by_index;
    std::vector<uint8_t> m_reclaimed_listener_indices;
};
