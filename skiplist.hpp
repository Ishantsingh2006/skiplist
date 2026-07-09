#ifndef SKIPLIST_HPP
#define SKIPLIST_HPP

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <shared_mutex>
#include <mutex>
#include <utility>
#include <functional>
#include <stdexcept>
#include <iterator>
#include <expected>
#include <atomic>

namespace skip
{

    struct NodeBase
    {
        std::vector<NodeBase *> forward;
        NodeBase *prev{nullptr};

        NodeBase(size_t height) : forward(height, nullptr) {}
        virtual ~NodeBase() = default;

        NodeBase(const NodeBase &) = delete;
        NodeBase &operator=(const NodeBase &) = delete;

#ifdef SKIPLIST_TEST_MEM
        static inline std::atomic<int64_t> active_nodes{0};

        void *operator new(size_t size)
        {
            active_nodes++;
            return ::operator new(size);
        }

        void operator delete(void *ptr) noexcept
        {
            if (ptr)
            {
                active_nodes--;
            }
            ::operator delete(ptr);
        }
#endif
    };

    template <typename Key, typename T>
    struct Node : public NodeBase
    {
        using value_type = std::pair<const Key, T>;
        value_type value;

        Node(const Key &k, const T &v, size_t height)
            : NodeBase(height), value(k, v) {}

        Node(Key &&k, T &&v, size_t height)
            : NodeBase(height), value(std::piecewise_construct,
                                      std::forward_as_tuple(std::move(k)),
                                      std::forward_as_tuple(std::move(v))) {}
    };

    template <typename Key, typename T>
    class SkipListIterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::pair<const Key, T>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type *;
        using reference = value_type &;

    private:
        NodeBase *m_node;
        NodeBase *m_head;

    public:
        explicit SkipListIterator(NodeBase *node, NodeBase *head) : m_node(node), m_head(head) {}

        reference operator*() const
        {
            return static_cast<Node<Key, T> *>(m_node)->value;
        }

        pointer operator->() const
        {
            return &(static_cast<Node<Key, T> *>(m_node)->value);
        }

        SkipListIterator &operator++()
        {
            if (m_node == m_head)
            {
                throw std::out_of_range("cannot increment end iterator");
            }
            m_node = m_node->forward[0];
            if (m_node == nullptr)
            {
                m_node = m_head;
            }
            return *this;
        }

        SkipListIterator operator++(int)
        {
            SkipListIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        SkipListIterator &operator--()
        {
            if (m_node == (m_head->forward[0] ? m_head->forward[0] : m_head))
            {
                throw std::out_of_range("cannot decrement begin iterator");
            }
            m_node = m_node->prev;
            return *this;
        }

        SkipListIterator operator--(int)
        {
            SkipListIterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const SkipListIterator &a, const SkipListIterator &b)
        {
            return a.m_node == b.m_node;
        }

        friend bool operator!=(const SkipListIterator &a, const SkipListIterator &b)
        {
            return a.m_node != b.m_node;
        }

        NodeBase *get_node() const { return m_node; }
        NodeBase *get_head() const { return m_head; }
    };

    template <typename Key, typename T>
    class SkipListConstIterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = const std::pair<const Key, T>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type *;
        using reference = value_type &;

    private:
        const NodeBase *m_node;
        const NodeBase *m_head;

    public:
        explicit SkipListConstIterator(const NodeBase *node, const NodeBase *head) : m_node(node), m_head(head) {}

        SkipListConstIterator(const SkipListIterator<Key, T> &other)
            : m_node(other.get_node()), m_head(other.get_head()) {}

        reference operator*() const
        {
            return static_cast<const Node<Key, T> *>(m_node)->value;
        }

        pointer operator->() const
        {
            return &(static_cast<const Node<Key, T> *>(m_node)->value);
        }

        SkipListConstIterator &operator++()
        {
            if (m_node == m_head)
            {
                throw std::out_of_range("cannot increment end iterator");
            }
            m_node = m_node->forward[0];
            if (m_node == nullptr)
            {
                m_node = m_head;
            }
            return *this;
        }

        SkipListConstIterator operator++(int)
        {
            SkipListConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        SkipListConstIterator &operator--()
        {
            if (m_node == (m_head->forward[0] ? m_head->forward[0] : m_head))
            {
                throw std::out_of_range("cannot decrement begin iterator");
            }
            m_node = m_node->prev;
            return *this;
        }

        SkipListConstIterator operator--(int)
        {
            SkipListConstIterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const SkipListConstIterator &a, const SkipListConstIterator &b)
        {
            return a.m_node == b.m_node;
        }

        friend bool operator!=(const SkipListConstIterator &a, const SkipListConstIterator &b)
        {
            return a.m_node != b.m_node;
        }

        const NodeBase *get_node() const { return m_node; }
        const NodeBase *get_head() const { return m_head; }
    };

    template <typename Key, typename T, typename Compare = std::less<Key>>
    class skiplist
    {
    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<const Key, T>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using key_compare = Compare;
        using iterator = SkipListIterator<Key, T>;
        using const_iterator = SkipListConstIterator<Key, T>;

        enum class IntegrityError
        {
            OK = 0,
            NOT_SORTED,
            INVALID_HEIGHT,
            COUNT_MISMATCH,
            FORWARD_POINTER_NULL_LEVEL_0
        };

    private:
        NodeBase m_head;
        size_type m_size;
        size_t m_max_level;
        float m_p;
        Compare m_compare;

        mutable std::shared_mutex m_mutex;
        mutable std::mt19937 m_rng;

        size_t random_level() const
        {
            size_t level = 1;
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            while (dist(m_rng) < m_p && level < m_max_level)
            {
                level++;
            }
            return level;
        }

        bool equivalent(const Key &a, const Key &b) const
        {
            return !m_compare(a, b) && !m_compare(b, a);
        }

        std::vector<NodeBase *> find_predecessors(const Key &key) const
        {
            std::vector<NodeBase *> update(m_max_level, nullptr);
            const NodeBase *curr = &m_head;
            for (int i = static_cast<int>(m_max_level) - 1; i >= 0; --i)
            {
                while (curr->forward[i] != nullptr)
                {
                    const Node<Key, T> *next_node = static_cast<const Node<Key, T> *>(curr->forward[i]);
                    if (m_compare(next_node->value.first, key))
                    {
                        curr = curr->forward[i];
                    }
                    else
                    {
                        break;
                    }
                }
                update[i] = const_cast<NodeBase *>(curr);
            }
            return update;
        }

        iterator find_existing(const Key &key, const std::vector<NodeBase *> &update)
        {
            NodeBase *next = update[0]->forward[0];
            if (next != nullptr)
            {
                Node<Key, T> *next_node = static_cast<Node<Key, T> *>(next);
                if (equivalent(key, next_node->value.first))
                {
                    return iterator(next, &m_head);
                }
            }
            return iterator(&m_head, &m_head);
        }

        iterator create_node(Key &&key, T &&value, const std::vector<NodeBase *> &update)
        {
            size_t lvl = random_level();
            Node<Key, T> *new_node = new Node<Key, T>(std::move(key), std::move(value), lvl);
            for (size_t i = 0; i < lvl; ++i)
            {
                new_node->forward[i] = update[i]->forward[i];
                update[i]->forward[i] = new_node;
            }

            new_node->prev = (update[0] == &m_head) ? nullptr : update[0];
            if (new_node->forward[0] != nullptr)
            {
                new_node->forward[0]->prev = new_node;
            }
            else
            {
                m_head.prev = new_node;
            }
            m_size++;
            return iterator(new_node, &m_head);
        }

        template <typename Self>
        auto *find_helper(this Self &&self, const Key &key)
        {
            using NodeBasePtr = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                                   const NodeBase *,
                                                   NodeBase *>;
            NodeBasePtr curr = &self.m_head;
            for (int i = static_cast<int>(self.m_max_level) - 1; i >= 0; --i)
            {
                while (curr->forward[i] != nullptr)
                {
                    using NodePtr = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                                       const Node<Key, T> *,
                                                       Node<Key, T> *>;
                    NodePtr next_node = static_cast<NodePtr>(curr->forward[i]);
                    if (self.m_compare(next_node->value.first, key))
                    {
                        curr = curr->forward[i];
                    }
                    else
                    {
                        break;
                    }
                }
            }
            NodeBasePtr next = curr->forward[0];
            if (next != nullptr)
            {
                using NodePtr = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                                   const Node<Key, T> *,
                                                   Node<Key, T> *>;
                NodePtr next_node = static_cast<NodePtr>(next);
                if (self.equivalent(key, next_node->value.first))
                {
                    return next;
                }
            }
            return static_cast<NodeBasePtr>(nullptr);
        }


        size_type erase_helper(const Key &key)
        {
            std::vector<NodeBase *> update = find_predecessors(key);
            NodeBase *curr = update[0];

            NodeBase *next = curr->forward[0];
            if (next == nullptr)
            {
                return 0;
            }

            Node<Key, T> *target_node = static_cast<Node<Key, T> *>(next);
            if (!equivalent(key, target_node->value.first))
            {
                return 0;
            }

            for (size_t i = 0; i < target_node->forward.size(); ++i)
            {
                update[i]->forward[i] = target_node->forward[i];
            }

            if (target_node->forward[0] != nullptr)
            {
                target_node->forward[0]->prev = target_node->prev;
            }
            else
            {
                m_head.prev = target_node->prev;
            }

            delete target_node;
            m_size--;
            return 1;
        }

        template <typename M>
        std::pair<iterator, bool> insert_or_assign(const Key &k, M &&obj)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            std::vector<NodeBase *> update = find_predecessors(k);
            iterator it = find_existing(k, update);
            if (it != iterator(&m_head, &m_head))
            {
                it->second = std::forward<M>(obj);
                return {it, false};
            }
            iterator new_it = create_node(Key(k), T(std::forward<M>(obj)), update);
            return {new_it, true};
        }

    public:
        explicit skiplist(size_t max_level = 32, float p = 0.5f, const Compare &comp = Compare())
            : m_head(max_level),
              m_size(0),
              m_max_level(max_level),
              m_compare(comp),
              m_rng(std::random_device{}())
        {
            if (p < 0.0f || p > 1.0f)
            {
                throw std::invalid_argument("Probability p must be between 0.0 and 1.0");
            }
            m_p = p;
            m_head.prev = nullptr;
        }

        ~skiplist()
        {
            clear();
        }

        skiplist(const skiplist &) = delete;
        skiplist &operator=(const skiplist &) = delete;

        skiplist(skiplist &&other) noexcept
        {
            std::unique_lock<std::shared_mutex> lock(other.m_mutex);
            m_max_level = other.m_max_level;
            m_p = other.m_p;
            m_compare = other.m_compare;
            m_size = other.m_size;
            m_head.forward = std::move(other.m_head.forward);
            m_head.prev = other.m_head.prev;

            other.m_head.forward.resize(other.m_max_level, nullptr);
            other.m_head.prev = nullptr;
            other.m_size = 0;
        }

        skiplist &operator=(skiplist &&other) noexcept
        {
            if (this != &other)
            {
                std::unique_lock<std::shared_mutex> lock1(m_mutex, std::defer_lock);
                std::unique_lock<std::shared_mutex> lock2(other.m_mutex, std::defer_lock);
                std::lock(lock1, lock2);

                clear();
                m_max_level = other.m_max_level;
                m_p = other.m_p;
                m_compare = other.m_compare;
                m_size = other.m_size;
                m_head.forward = std::move(other.m_head.forward);
                m_head.prev = other.m_head.prev;

                other.m_head.forward.resize(other.m_max_level, nullptr);
                other.m_head.prev = nullptr;
                other.m_size = 0;
            }
            return *this;
        }

        // Capacity
        bool empty() const noexcept
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_size == 0;
        }

        size_type size() const noexcept
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_size;
        }

        size_t max_level() const noexcept
        {
            return m_max_level;
        }

        // Modifiers
        void clear() noexcept
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            NodeBase *curr = m_head.forward[0];
            while (curr != nullptr)
            {
                NodeBase *next = curr->forward[0];
                delete static_cast<Node<Key, T> *>(curr);
                curr = next;
            }
            std::fill(m_head.forward.begin(), m_head.forward.end(), nullptr);
            m_head.prev = nullptr;
            m_size = 0;
        }

        std::pair<iterator, bool> insert(const value_type &val)
        {
            return insert_or_assign(val.first, val.second);
        }

        std::pair<iterator, bool> insert(value_type &&val)
        {
            return insert_or_assign(val.first, std::move(val.second));
        }



        size_type erase(const Key &key)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            return erase_helper(key);
        }

        iterator erase(const_iterator pos)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            if (pos == end())
            {
                return end();
            }
            const Key &key = pos->first;
            NodeBase *next_pos = const_cast<NodeBase *>(pos.get_node())->forward[0];
            erase_helper(key);
            return iterator(next_pos, &m_head);
        }

        // Element Access
        template <typename Self>
        auto &&at(this Self &&self, const Key &key)
        {
            std::shared_lock<std::shared_mutex> lock(self.m_mutex);
            auto *found = self.find_helper(key);
            if (found == nullptr)
            {
                throw std::out_of_range("skiplist::at: key not found");
            }
            using NodePtr = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                               const Node<Key, T> *,
                                               Node<Key, T> *>;
            return static_cast<NodePtr>(found)->value.second;
        }

        T &operator[](const Key &key)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            std::vector<NodeBase *> update = find_predecessors(key);
            iterator it = find_existing(key, update);
            if (it != iterator(&m_head, &m_head))
            {
                return it->second;
            }
            iterator new_it = create_node(Key(key), T(), update);
            return new_it->second;
        }

        // Lookup
        template <typename Self>
        auto find(this Self &&self, const Key &key)
        {
            std::shared_lock<std::shared_mutex> lock(self.m_mutex);
            auto *found = self.find_helper(key);
            using IteratorType = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                                    const_iterator,
                                                    iterator>;
            return found ? IteratorType(found, &self.m_head) : self.end();
        }

        size_type count(const Key &key) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return find_helper(key) ? 1 : 0;
        }

        bool contains(const Key &key) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return find_helper(key) != nullptr;
        }

        // Iterators
        template <typename Self>
        auto begin(this Self &&self) noexcept
        {
            std::shared_lock<std::shared_mutex> lock(self.m_mutex);
            using IteratorType = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                                    const_iterator,
                                                    iterator>;
            return IteratorType(self.m_head.forward[0] ? self.m_head.forward[0] : &self.m_head, &self.m_head);
        }

        const_iterator cbegin() const noexcept
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return const_iterator(m_head.forward[0] ? m_head.forward[0] : &m_head, &m_head);
        }

        template <typename Self>
        auto end(this Self &&self) noexcept
        {
            using IteratorType = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                                    const_iterator,
                                                    iterator>;
            return IteratorType(&self.m_head, &self.m_head);
        }

        const_iterator cend() const noexcept
        {
            return const_iterator(&m_head, &m_head);
        }

        // Visualization and Diagnostics
        void print_level(size_t level, std::ostream &os = std::cout) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (level >= m_max_level)
                return;

            const NodeBase *curr = m_head.forward[level];
            bool first = true;
            while (curr != nullptr)
            {
                const Node<Key, T> *node = static_cast<const Node<Key, T> *>(curr);
                if (!first)
                {
                    os << " ";
                }
                os << node->value.first;
                first = false;
                curr = curr->forward[level];
            }
            os << "\n";
        }

        int get_node_height(const Key &key) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            const NodeBase *found = find_helper(key);
            return found ? static_cast<int>(found->forward.size()) : -1;
        }

        std::expected<void, IntegrityError> lacksIntegrity() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);

            if (m_size == 0)
            {
                for (size_t i = 0; i < m_max_level; ++i)
                {
                    if (m_head.forward[i] != nullptr)
                    {
                        return std::unexpected(IntegrityError::FORWARD_POINTER_NULL_LEVEL_0);
                    }
                }
                if (m_head.prev != nullptr)
                {
                    return std::unexpected(IntegrityError::COUNT_MISMATCH);
                }
                return {};
            }

            size_type count = 0;
            const NodeBase *curr = m_head.forward[0];
            const Node<Key, T> *prev = nullptr;
            while (curr != nullptr)
            {
                const Node<Key, T> *node = static_cast<const Node<Key, T> *>(curr);

                if (node->forward.empty() || node->forward.size() > m_max_level)
                {
                    return std::unexpected(IntegrityError::INVALID_HEIGHT);
                }

                if ((prev == nullptr && node->prev != nullptr) || (prev != nullptr && node->prev != prev))
                {
                    return std::unexpected(IntegrityError::FORWARD_POINTER_NULL_LEVEL_0);
                }

                if (prev != nullptr)
                {
                    if (!m_compare(prev->value.first, node->value.first))
                    {
                        return std::unexpected(IntegrityError::NOT_SORTED);
                    }
                }

                prev = node;
                curr = curr->forward[0];
                count++;
            }

            if (count != m_size)
            {
                return std::unexpected(IntegrityError::COUNT_MISMATCH);
            }
            if (m_head.prev != prev)
            {
                return std::unexpected(IntegrityError::COUNT_MISMATCH);
            }

            for (size_t i = 1; i < m_max_level; ++i)
            {
                const NodeBase *curr_i = m_head.forward[i];
                const NodeBase *curr_0 = &m_head;

                while (curr_i != nullptr)
                {
                    while (curr_0 != nullptr && curr_0 != curr_i)
                    {
                        curr_0 = curr_0->forward[0];
                    }
                    if (curr_0 == nullptr)
                    {
                        return std::unexpected(IntegrityError::NOT_SORTED);
                    }
                    if (curr_i->forward.size() <= i)
                    {
                        return std::unexpected(IntegrityError::INVALID_HEIGHT);
                    }
                    curr_i = curr_i->forward[i];
                }
            }

            return {};
        }

        void print_distribution(std::ostream &os = std::cout) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);

            std::vector<size_t> level_counts(m_max_level, 0);
            NodeBase *curr = m_head.forward[0];
            while (curr != nullptr)
            {
                Node<Key, T> *node = static_cast<Node<Key, T> *>(curr);
                size_t height = node->forward.size();
                for (size_t i = 0; i < height; ++i)
                {
                    level_counts[i]++;
                }
                curr = curr->forward[0];
            }

            auto get_percentage = [this](size_t count) -> double
            {
                return m_size ? (100.0 * count / m_size) : 0.0;
            };

            for (size_t i = 0; i < m_max_level; ++i)
            {
                if (i > 0 && level_counts[i] == 0)
                    continue;
                os << "Level " << i << ": " << level_counts[i] << " nodes ("
                   << get_percentage(level_counts[i]) << "%)\n";
            }
        }
    };

}

#endif // SKIPLIST_HPP
