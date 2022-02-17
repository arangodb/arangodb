// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_TRIE_MAP_HPP
#define BOOST_TEXT_TRIE_MAP_HPP

#include <boost/text/trie.hpp>

#include <boost/stl_interfaces/reverse_iterator.hpp>


namespace boost { namespace text {

    template<typename Key, typename Value>
    struct trie_map_iterator;

    template<typename Key, typename Value>
    struct const_trie_map_iterator;

    template<typename Key, typename Value>
    using reverse_trie_map_iterator =
        stl_interfaces::reverse_iterator<trie_map_iterator<Key, Value>>;

    template<typename Key, typename Value>
    using const_reverse_trie_map_iterator =
        stl_interfaces::reverse_iterator<const_trie_map_iterator<Key, Value>>;

    /** A range type returned by certain operations on a trie_map or
        trie_set. */
    template<typename Iter>
    struct trie_range
    {
        using iterator = Iter;

        iterator first;
        iterator last;

        iterator begin() const { return first; }
        iterator end() const { return last; }

        friend bool operator==(trie_range const & lhs, trie_range const & rhs)
        {
            return lhs.first == rhs.first && lhs.last == rhs.last;
        }
        friend bool operator!=(trie_range const & lhs, trie_range const & rhs)
        {
            return !(lhs == rhs);
        }
    };

    /** A constant range type returned by certain operations on a trie_map or
        trie_set. */
    template<typename Iter>
    struct const_trie_range
    {
        using const_iterator = Iter;

        const_iterator first;
        const_iterator last;

        const_iterator begin() const { return first; }
        const_iterator end() const { return last; }

        friend bool
        operator==(const_trie_range const & lhs, const_trie_range const & rhs)
        {
            return lhs.first == rhs.first && lhs.last == rhs.last;
        }
        friend bool
        operator!=(const_trie_range const & lhs, const_trie_range const & rhs)
        {
            return !(lhs == rhs);
        }
    };

    /** The result type of insert() operations on a trie_map or trie_set. */
    template<typename Iter>
    struct trie_insert_result
    {
        using iterator = Iter;

        trie_insert_result() : iter(), inserted(false) {}
        trie_insert_result(iterator it, bool ins) : iter(it), inserted(ins) {}

        iterator iter;
        bool inserted;

        friend bool operator==(
            trie_insert_result const & lhs, trie_insert_result const & rhs)
        {
            return lhs.iter == rhs.iter && lhs.inserted == rhs.inserted;
        }
        friend bool operator!=(
            trie_insert_result const & lhs, trie_insert_result const & rhs)
        {
            return !(lhs == rhs);
        }
    };

    namespace detail {

        struct index_within_parent_t
        {
            index_within_parent_t() : value_(-1) {}

            std::size_t value() const noexcept { return value_; }

            template<
                typename Key,
                typename Value,
                typename Iter,
                std::size_t KeySize>
            void insert_at(
                std::unique_ptr<trie_node_t<
                    index_within_parent_t,
                    Key,
                    Value,
                    KeySize>> const & child,
                std::ptrdiff_t offset,
                Iter it,
                Iter end)
            {
                child->index_within_parent_.value_ = offset;
                for (; it != end; ++it) {
                    ++(*it)->index_within_parent_.value_;
                }
            }

            template<typename Key, typename Value, std::size_t KeySize>
            void insert_ptr(std::unique_ptr<trie_node_t<
                                index_within_parent_t,
                                Key,
                                Value,
                                KeySize>> const & child)
            {
                child->index_within_parent_.value_ = 0;
            }

            template<typename Iter>
            void erase(Iter it, Iter end)
            {
                for (; it != end; ++it) {
                    --(*it)->index_within_parent_.value_;
                }
            }

        private:
            std::size_t value_;
        };

        template<typename Key, typename Value, std::size_t KeySize = 0>
        struct trie_iterator_state_t
        {
            trie_node_t<index_within_parent_t, Key, Value, KeySize> const *
                parent_;
            std::size_t index_;
        };

        template<typename Key, typename Value, std::size_t KeySize>
        trie_iterator_state_t<Key, Value, KeySize>
        parent_state(trie_iterator_state_t<Key, Value, KeySize> state)
        {
            return {state.parent_->parent(),
                    state.parent_->index_within_parent()};
        }

        template<typename Key, typename Value, std::size_t KeySize>
        Key reconstruct_key(
            trie_iterator_state_t<Key, Value, KeySize>
                state) noexcept(noexcept(std::declval<Key &>()
                                             .insert(
                                                 std::declval<Key &>().end(),
                                                 state.parent_->key(
                                                     state.index_))))
        {
            Key retval;
            while (state.parent_->parent()) {
                retval.insert(retval.end(), state.parent_->key(state.index_));
                state = parent_state(state);
            }
            std::reverse(retval.begin(), retval.end());
            return retval;
        }

        template<typename Key, typename Value, std::size_t KeySize>
        trie_node_t<index_within_parent_t, Key, Value, KeySize> const *
        to_node(trie_iterator_state_t<Key, Value, KeySize> state)
        {
            if (state.index_ < state.parent_->size())
                return state.parent_->child(state.index_);
            return nullptr;
        }
    }

    // TODO: KeyIter, KeyRange, and Range concepts.
    // TODO: Key concept specifying that Key is a container.
    // TODO: Compare concept specifying that Compare compares Key::value_types.
    // Don't forget to mention that Compare must be statically polymorphic.

    /** An associative container similar to std::map, built upon a trie, or
        prefix-tree.  A trie_map contains a set of keys, each of which is a
        sequence, and a set of values, each associated with some key.  Each
        node in the trie_map represents some prefix found in at least one
        member of the set of values contained in the trie_map.  If a certain
        node represents the end of one of the keys, it has an associated
        value.  Such a node may or may not have children.

        Complexity of lookups is always O(M), where M is the size of the Key
        being lookep up.  Note that this implies that lookup complexity is
        independent of the size of the trie_map.

        \param Key The key-type; must be a sequence of values comparable via
        Compare()(x, y).
        \param Value The value-type.
        \param Compare The type of the comparison object used to compare
        elements of the key-type.
    */
    template<typename Key, typename Value, typename Compare>
    struct trie_map
    {
    private:
        using node_t =
            detail::trie_node_t<detail::index_within_parent_t, Key, Value>;
        using iter_state_t = detail::trie_iterator_state_t<Key, Value>;

    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = trie_element<Key, Value>;
        using key_compare = Compare;
        using key_element_type = typename Key::value_type;

        using reference = value_type &;
        using const_reference = value_type const &;
        using iterator = trie_map_iterator<key_type, mapped_type>;
        using const_iterator = const_trie_map_iterator<key_type, mapped_type>;
        using reverse_iterator =
            reverse_trie_map_iterator<key_type, mapped_type>;
        using const_reverse_iterator =
            const_reverse_trie_map_iterator<key_type, mapped_type>;
        using size_type = std::ptrdiff_t;
        using difference_type = std::ptrdiff_t;

        using range = trie_range<iterator>;
        using const_range = const_trie_range<const_iterator>;
        using insert_result = trie_insert_result<iterator>;
        using match_result = trie_match_result;

        trie_map() : size_(0) {}

        trie_map(Compare const & comp) : size_(0), comp_(comp) {}

        template<typename Iter, typename Sentinel>
        trie_map(Iter first, Sentinel last, Compare const & comp = Compare()) :
            size_(0),
            comp_(comp)
        {
            insert(first, last);
        }
        template<typename Range>
        explicit trie_map(Range r, Compare const & comp = Compare()) :
            size_(0),
            comp_(comp)
        {
            insert(std::begin(r), std::end(r));
        }
        template<std::size_t KeySize>
        explicit trie_map(
            boost::text::trie<Key, Value, Compare, KeySize> const & trie) :
            size_(0)
        {
            Key key;
            from_trie_impl(trie.header_, key);
        }
        trie_map(std::initializer_list<value_type> il) : size_(0)
        {
            insert(il);
        }

        trie_map & operator=(std::initializer_list<value_type> il)
        {
            clear();
            for (auto const & x : il) {
                insert(x);
            }
            return *this;
        }

        bool empty() const noexcept { return size_ == 0; }
        size_type size() const noexcept { return size_; }
        size_type max_size() const noexcept { return PTRDIFF_MAX; }

        const_iterator begin() const noexcept
        {
            iter_state_t state{&header_, 0};
            if (size_) {
                while (!state.parent_->min_value()) {
                    state.parent_ = state.parent_->min_child();
                }
            }
            return const_iterator(state);
        }
        const_iterator end() const noexcept
        {
            iter_state_t state{&header_, 0};
            if (size_) {
                node_t const * node = nullptr;
                while ((node = to_node(state))) {
                    state.parent_ = node;
                    state.index_ = state.parent_->size() - 1;
                }
                state.parent_ = state.parent_->parent();
                state.index_ = state.parent_->size();
            }
            return const_iterator(state);
        }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }

        const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator{end()};
        }
        const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator{begin()};
        }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

#ifndef BOOST_TEXT_DOXYGEN

#define BOOST_TRIE_MAP_C_STR_OVERLOAD(rtype, func, quals)                      \
    template<typename Char, std::size_t N>                                     \
    rtype func(Char const(&chars)[N]) quals                                    \
    {                                                                          \
        static_assert(                                                         \
            std::is_same<Char, key_element_type>::value,                       \
            "Only well-formed when Char is Key::value_type.");                 \
        return func(detail::char_range<Char const>{chars, chars + N - 1});     \
    }

#endif

        /** Returns true if `key` is found in *this. */
        template<typename KeyRange>
        bool contains(KeyRange const & key) const noexcept
        {
            return find(key) != end();
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(bool, contains, const noexcept)
#endif

        /** Returns the iterator pointing to the key/value pair associated
            with `key`, if `key` is found in *this.  Returns end()
            otherwise. */
        template<typename KeyRange>
        const_iterator find(KeyRange const & key) const noexcept
        {
            auto first = std::begin(key);
            auto const last = std::end(key);
            auto match = longest_match_impl(first, last);
            if (first == last && match.match) {
                return const_iterator(iter_state_t{
                    to_node_ptr(match.node)->parent(),
                    to_node_ptr(match.node)->index_within_parent()});
            }
            return this->end();
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(const_iterator, find, const noexcept)
#endif

        /** Returns the iterator pointing to the first key/value pair whose
            key is not less than `key`.  Returns end() if no such key can be
            found. */
        template<typename KeyRange>
        const_iterator lower_bound(KeyRange const & key) const noexcept
        {
            return bound_impl<true>(key);
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(
            const_iterator, lower_bound, const noexcept)
#endif

        /** Returns the iterator pointing to the first key/value pair whose
            key is not greater than `key`.  Returns end() if no such key can
            be found. */
        template<typename KeyRange>
        const_iterator upper_bound(KeyRange const & key) const noexcept
        {
            return bound_impl<false>(key);
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(
            const_iterator, upper_bound, const noexcept)
#endif

        /** Returns the `const_range(lower_bound(key), upper_bound(key))`.*/
        template<typename KeyRange>
        const_range equal_range(KeyRange const & key) const noexcept
        {
            return {lower_bound(key), upper_bound(key)};
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(const_range, equal_range, const noexcept)
#endif

        /** Returns the longest subsequence of `[first, last)` found in *this,
            whether or not it is a match. */
        template<typename KeyIter, typename Sentinel>
        match_result longest_subsequence(KeyIter first, Sentinel last) const
            noexcept
        {
            return longest_match_impl(first, last);
        }

        /** Returns the longest subsequence of `key` found in *this, whether
            or not it is a match. */
        template<typename KeyRange>
        match_result longest_subsequence(KeyRange const & key) const noexcept
        {
            return longest_subsequence(std::begin(key), std::end(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(
            match_result, longest_subsequence, const noexcept)
#endif

        /** Returns the longest matching subsequence of `[first, last)` found
            in *this. */
        template<typename KeyIter, typename Sentinel>
        match_result longest_match(KeyIter first, Sentinel last) const noexcept
        {
            auto const retval = longest_match_impl(first, last);
            return back_up_to_match(retval);
        }

        /** Returns the longest matching subsequence of `key` found in
            *this. */
        template<typename KeyRange>
        match_result longest_match(KeyRange const & key) const noexcept
        {
            return longest_match(std::begin(key), std::end(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(
            match_result, longest_match, const noexcept)
#endif

        /** Returns the result of extending `prev` by one element, `e`. */
        template<typename KeyElementT>
        match_result extend_subsequence(match_result prev, KeyElementT e) const
            noexcept
        {
            auto e_ptr = &e;
            return extend_subsequence_impl(prev, e_ptr, e_ptr + 1);
        }

        /** Returns the result of extending `prev` by the longest subsequence
            of `[first, last)` found in *this. */
        template<typename KeyIter, typename Sentinel>
        match_result extend_subsequence(
            match_result prev, KeyIter first, Sentinel last) const noexcept
        {
            return extend_subsequence_impl(prev, first, last);
        }

        /** Returns the result of extending `prev` by one element, `e`, if
           that would form a match, and `prev` otherwise.  `prev` must be a
           match. */
        template<typename KeyElementT>
        match_result extend_match(match_result prev, KeyElementT e) const
            noexcept
        {
            BOOST_ASSERT(prev.match);
            auto e_ptr = &e;
            auto const retval = extend_subsequence_impl(prev, e_ptr, e_ptr + 1);
            return back_up_to_match(retval);
        }

        /** Returns the result of extending `prev` by the longest subsequence
            of `[first, last)` found in *this, if that would form a match, and
            `prev` otherwise.  `prev` must be a match. */
        template<typename KeyIter, typename Sentinel>
        match_result
        extend_match(match_result prev, KeyIter first, Sentinel last) const
            noexcept
        {
            BOOST_ASSERT(prev.match);
            auto const retval = extend_subsequence_impl(prev, first, last);
            return back_up_to_match(retval);
        }

        /** Writes the sequence of elements that would advance `prev` by one
            element to `out`, and returns the final value of `out` after the
            writes. */
        template<typename OutIter>
        OutIter copy_next_key_elements(match_result prev, OutIter out) const
        {
            auto node = to_node_ptr(prev.node);
            return std::copy(node->key_begin(), node->key_end(), out);
        }

        /** Returns an optional reference to the const value associated with
            `key` in *this (if any). */
        template<typename KeyRange>
        optional_ref<mapped_type const> operator[](KeyRange const & key) const
            noexcept
        {
            auto it = find(key);
            if (it == end())
                return {};
            return it->value;
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(
            optional_ref<mapped_type const>, operator[], const noexcept)
#endif

        /** Returns an optional reference to the const value associated with
            `match` in *this (if any). */
        optional_ref<mapped_type const> operator[](match_result match) const
            noexcept
        {
            if (!match.match)
                return {};
            return *to_node_ptr(match.node)->value();
        }

        iterator begin() noexcept { return iterator(const_this()->begin()); }
        iterator end() noexcept { return iterator(const_this()->end()); }

        reverse_iterator rbegin() noexcept { return reverse_iterator{end()}; }
        reverse_iterator rend() noexcept { return reverse_iterator{begin()}; }

        void clear() noexcept
        {
            header_ = node_t();
            size_ = 0;
        }

        /** Returns the iterator pointing to the key/value pair associated
            with `key`, if `key` is found in *this.  Returns end()
            otherwise. */
        template<typename KeyRange>
        iterator find(KeyRange const & key) noexcept
        {
            return iterator(const_this()->find(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(iterator, find, noexcept)
#endif

        /** Returns the iterator pointing to the first key/value pair whose
            key is not less than `key`.  Returns end() if no such key can be
            found. */
        template<typename KeyRange>
        iterator lower_bound(KeyRange const & key) noexcept
        {
            return iterator(const_this()->lower_bound(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(iterator, lower_bound, noexcept)
#endif

        /** Returns the iterator pointing to the first key/value pair whose
            key is not greater than `key`.  Returns end() if no such key can
            be found. */
        template<typename KeyRange>
        iterator upper_bound(KeyRange const & key) noexcept
        {
            return iterator(const_this()->upper_bound(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(iterator, upper_bound, noexcept)
#endif

        /** Returns the `const_range(lower_bound(key), upper_bound(key))`.*/
        template<typename KeyRange>
        range equal_range(KeyRange const & key) noexcept
        {
            return {lower_bound(key), upper_bound(key)};
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(range, equal_range, noexcept)
#endif

        /** Returns an optional reference to the value associated with `key`
            in *this (if any). */
        template<typename KeyRange>
        optional_ref<mapped_type> operator[](KeyRange const & key) noexcept
        {
            auto it = find(key);
            if (it == end())
                return {};
            return it->value;
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(
            optional_ref<mapped_type>, operator[], noexcept)
#endif

        /** Returns an optional reference to the value associated with `match`
            in *this (if any). */
        optional_ref<mapped_type> operator[](match_result match) noexcept
        {
            if (!match.match)
                return {};
            return *const_cast<node_t *>(to_node_ptr(match.node))->value();
        }

        /** Inserts the key/value pair `[first, last)`, `value` into *this.
            The `inserted` field of the result will be true if the operation
            resulted in a new insertion, or false otherwise. */
        template<typename KeyIter, typename Sentinel>
        insert_result insert(KeyIter first, Sentinel last, Value value)
        {
            if (empty()) {
                std::unique_ptr<node_t> new_node(new node_t(&header_));
                header_.insert(std::move(new_node));
            }

            auto match = longest_match_impl(first, last);
            if (first == last && match.match) {
                return {iterator(iter_state_t{
                            to_node_ptr(match.node)->parent(),
                            to_node_ptr(match.node)->index_within_parent()}),
                        false};
            }
            auto node = create_children(
                const_cast<node_t *>(to_node_ptr(match.node)), first, last);
            node->value() = std::move(value);
            ++size_;
            return {iterator(iter_state_t{node->parent(), 0}), true};
        }

        /** Inserts the key/value pair `key`, `value` into *this.  The
            `inserted` field of the result will be true if the operation
            resulted in a new insertion, or false otherwise. */
        template<typename KeyRange>
        insert_result insert(KeyRange const & key, Value value)
        {
            return insert(std::begin(key), std::end(key), std::move(value));
        }

        template<typename Char, std::size_t N>
        insert_result insert(Char const (&chars)[N], Value value)
        {
            static_assert(
                std::is_same<Char, key_element_type>::value,
                "Only well-formed when Char is Key::value_type.");
            return insert(
                detail::char_range<Char const>{chars, chars + N - 1},
                std::move(value));
        }

        /** Inserts the key/value pair `e` into *this.  The `inserted` field
            of the result will be true if the operation resulted in a new
            insertion, or false otherwise. */
        insert_result insert(value_type e)
        {
            return insert(
                std::begin(e.key), std::end(e.key), std::move(e.value));
        }

        /** Inserts the sequence of key/value pairs `[first, last)` into
            *this.  The `inserted` field of the result will be true if the
            operation resulted in a new insertion, or false otherwise. */
        template<typename Iter, typename Sentinel>
        void insert(Iter first, Sentinel last)
        {
            for (; first != last; ++first) {
                insert(first->key, first->value);
            }
        }

        /** Inserts the sequence of key/value pairs `r` into *this.  The
            `inserted` field of the result will be true if the operation
            resulted in a new insertion, or false otherwise. */
        template<typename Range>
        insert_result insert(Range const & r)
        {
            return insert(std::begin(r), std::end(r));
        }

        /** Inserts the sequence of key/value pairs `il` into this.  The
            `inserted` field of the result will be true if the operation
            resulted in a new insertion, or false otherwise. */
        void insert(std::initializer_list<value_type> il)
        {
            for (auto const & x : il) {
                insert(x);
            }
        }

        /** Inserts the key/value pair `[first, last)`, `value` into *this, or
            assigns `value` over the existing value associated with `[first,
            last)`, if this key is already found in *this.  The `inserted`
            field of the result will be true if the operation resulted in a
            new insertion, or false otherwise. */
        template<typename KeyIter, typename Sentinel>
        insert_result
        insert_or_assign(KeyIter first, Sentinel last, Value value)
        {
            auto it = first;
            auto match = longest_match_impl(it, last);
            if (it == last && match.match) {
                const_cast<Value &>(*to_node_ptr(match.node)->value()) =
                    std::move(value);
                return insert_result{
                    iterator(iter_state_t{
                        to_node_ptr(match.node)->parent(),
                        to_node_ptr(match.node)->index_within_parent()}),
                    false};
            }
            return insert(first, last, std::move(value));
        }

        /** Inserts the key/value pair `key`, `value` into *this, or assigns
            `value` over the existing value associated with `key`, if `key` is
            already found in *this.  The `inserted` field of the result will
            be true if the operation resulted in a new insertion, or false
            otherwise. */
        template<typename KeyRange>
        insert_result insert_or_assign(KeyRange const & key, Value value)
        {
            return insert_or_assign(
                std::begin(key), std::end(key), std::move(value));
        }

        template<typename Char, std::size_t N>
        insert_result insert_or_assign(Char const (&chars)[N], Value value)
        {
            static_assert(
                std::is_same<Char, key_element_type>::value,
                "Only well-formed when Char is Key::value_type.");
            return insert_or_assign(
                detail::char_range<Char const>{chars, chars + N - 1},
                std::move(value));
        }

        /** Erases the key/value pair associated with `key` from
            *this. Returns true if the key is found in *this, false
            otherwise. */
        template<typename KeyRange>
        bool erase(KeyRange const & key) noexcept
        {
            auto it = find(key);
            if (it == end())
                return false;
            erase(it);
            return true;
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_MAP_C_STR_OVERLOAD(bool, erase, noexcept)
#endif

        /** Erases the key/value pair pointed to by `it` from *this.  Returns
            an iterator to the next key/value pair in this. */
        iterator erase(iterator it)
        {
            auto state = it.it_.state_;

            --size_;

            auto node =
                const_cast<node_t *>(state.parent_->child(state.index_));
            if (!node->empty()) {
                // node has a value, but also children.  Remove the value and
                // return the next-iterator.
                ++it;
                node->value() = optional<Value>();
                return it;
            }

            // node has a value, *and* no children.  Remove it and all its
            // singular predecessors.
            const_cast<node_t *>(state.parent_)->erase(state.index_);
            while (state.parent_->parent() && state.parent_->empty() &&
                   !state.parent_->value()) {
                state = parent_state(state);
                const_cast<node_t *>(state.parent_)->erase(state.index_);
            }

            if (state.parent_->parent())
                state = parent_state(state);
            auto retval = iterator(state);
            if (!empty())
                ++retval;

            return retval;
        }

        /** Erases the sequence of key/value pairs pointed to by `[first,
            last)` from *this.  Returns an iterator to the next key/value pair
            in *this. */
        iterator erase(iterator first, iterator last)
        {
            auto retval = last;
            if (first == last)
                return retval;
            --last;
            while (last != first) {
                erase(last--);
            }
            erase(last);
            return retval;
        }

        void swap(trie_map & other)
        {
            header_.swap(other.header_);
            std::swap(size_, other.size_);
            std::swap(comp_, other.comp_);
        }

        friend bool operator==(trie_map const & lhs, trie_map const & rhs)
        {
            return lhs.size() == rhs.size() &&
                   std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }
        friend bool operator!=(trie_map const & lhs, trie_map const & rhs)
        {
            return !(lhs == rhs);
        }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        trie_map const * const_this()
        {
            return const_cast<trie_map const *>(this);
        }
        static node_t const * to_node_ptr(void const * ptr)
        {
            return static_cast<node_t const *>(ptr);
        }

        template<std::size_t KeySize>
        void from_trie_impl(
            detail::trie_node_t<
                detail::no_index_within_parent_t,
                Key,
                Value,
                KeySize> const & node,
            Key & key,
            bool root = true)
        {
            // TODO: Use an iterative approach instead?

            if (!!node.value()) {
                insert(key, *node.value());
            }

            std::vector<key_element_type> next_elements;
            node.copy_next_key_elements(std::back_inserter(next_elements));
            for (auto const & e : next_elements) {
                auto const * n = node.child(e, comp_);
                if (!root)
                    key.insert(key.end(), e);
                from_trie_impl(*n, key, false);
                if (!root)
                    key.erase(std::prev(key.end()));
            }
        }

        template<typename KeyIter, typename Sentinel>
        match_result longest_match_impl(KeyIter & first, Sentinel last) const
            noexcept
        {
            return extend_subsequence_impl(
                match_result{&header_, 0, false, true}, first, last);
        }

        template<typename KeyIter, typename Sentinel>
        match_result extend_subsequence_impl(
            match_result prev, KeyIter & first, Sentinel last) const noexcept
        {
            if (to_node_ptr(prev.node) == &header_) {
                if (header_.empty())
                    return prev;
                prev.node = header_.child(0);
            }

            if (first == last) {
                prev.match = !!to_node_ptr(prev.node)->value();
                prev.leaf = to_node_ptr(prev.node)->empty();
                return prev;
            }

            node_t const * node = to_node_ptr(prev.node);
            size_type size = prev.size;
            while (first != last) {
                auto const it = node->find(*first, comp_);
                if (it == node->end())
                    break;
                ++first;
                ++size;
                node = it->get();
            }

            return match_result{node, size, !!node->value(), node->empty()};
        }

        static match_result back_up_to_match(match_result retval) noexcept
        {
            auto node = to_node_ptr(retval.node);
            while (node->parent() && !node->value()) {
                retval.node = node = node->parent();
                --retval.size;
            }
            if (!!node->value())
                retval.match = true;
            return retval;
        }

        template<typename KeyIter, typename Sentinel>
        node_t * create_children(node_t * node, KeyIter first, Sentinel last)
        {
            auto retval = node;
            for (; first != last; ++first) {
                std::unique_ptr<node_t> new_node(new node_t(retval));
                retval =
                    retval->insert(*first, comp_, std::move(new_node))->get();
            }
            return retval;
        }

        template<bool LowerBound, typename KeyRange>
        const_iterator bound_impl(KeyRange const & key) const noexcept
        {
            auto first = std::begin(key);
            auto const last = std::end(key);
            auto match = longest_match_impl(first, last);
            if (first == last && match.match) {
                auto retval = const_iterator(iter_state_t{
                    to_node_ptr(match.node)->parent(),
                    to_node_ptr(match.node)->index_within_parent()});
                if (!LowerBound)
                    ++retval;
                return retval;
            }

            auto node = to_node_ptr(match.node);
            if (node->before_child_subtree(*first)) {
                // If the next element of the key would be before this node's
                // children, use the successor of this node; let
                // const_iterator::operator++() figure out for us which node
                // that is.
                return ++const_iterator(
                    iter_state_t{node->parent(), node->index_within_parent()});
            }

            auto const it = node->lower_bound(*first, comp_);
            if (it == node->end()) {
                // Find the max value in this subtree, and go one past that.
                do {
                    node = to_node_ptr(node->max_child());
                } while (!node->value());
                return ++const_iterator(
                    iter_state_t{node->parent(), node->parent()->size() - 1});
            }

            // Otherwise, find the min value within the child found above.
            std::size_t parent_index = it - node->begin();
            node = to_node_ptr(it->get());
            while (!node->value()) {
                node = to_node_ptr(node->min_child());
                parent_index = 0;
            }
            return const_iterator(iter_state_t{node->parent(), parent_index});
        }

        node_t header_;
        size_type size_;
        key_compare comp_;

#endif
    };

    template<typename Key, typename Compare>
    struct trie_set;

    template<typename Key>
    struct const_trie_set_iterator;

    template<typename Key, typename Value>
    struct const_trie_map_iterator : stl_interfaces::iterator_interface<
                                         const_trie_map_iterator<Key, Value>,
                                         std::bidirectional_iterator_tag,
                                         trie_element<Key, Value>,
                                         trie_element<Key, Value const &>>
    {
    private:
        using state_t = detail::trie_iterator_state_t<Key, Value>;
        state_t state_;
        using ref_type = trie_element<Key, Value const &>;
        using ptr_type = stl_interfaces::proxy_arrow_result<ref_type>;

    public:
        const_trie_map_iterator() noexcept : state_{nullptr, 0} {}

        const_trie_map_iterator(trie_match_result match_result) noexcept
        {
            BOOST_ASSERT(match_result.node);
            BOOST_ASSERT(match_result.match);
            auto node = static_cast<detail::trie_node_t<
                detail::index_within_parent_t,
                Key,
                Value> const *>(match_result.node);
            state_.parent_ = node->parent();
            state_.index_ = node->index_within_parent();
        }

        ref_type operator*() const
            noexcept(noexcept(detail::reconstruct_key(state_)))
        {
            return ref_type{detail::reconstruct_key(state_),
                            state_.parent_->child_value(state_.index_)};
        }
        ptr_type operator->() const
            noexcept(noexcept(detail::reconstruct_key(state_)))
        {
            ref_type && deref_result = **this;
            return ptr_type(
                ref_type{std::move(deref_result.key), deref_result.value});
         }

        const_trie_map_iterator & operator++() noexcept
        {
            auto node = to_node(state_);
            if (node && !node->empty()) {
                state_.parent_ = node;
                state_.index_ = 0;
            } else {
                // Try the next sibling node.
                ++state_.index_;
                auto const first_state = state_;
                while (state_.parent_->parent() &&
                       state_.parent_->parent()->parent() &&
                       state_.parent_->size() <= state_.index_) {
                    state_ = parent_state(state_);
                    ++state_.index_;
                }

                // If we went all the way up, incrementing indices, and they
                // were all at size() for each node, the first increment above
                // must have taken us to the end; use that.
                if ((!state_.parent_->parent() ||
                     !state_.parent_->parent()->parent()) &&
                    state_.parent_->size() <= state_.index_) {
                    state_ = first_state;
                    return *this;
                }
            }

            node = state_.parent_->child(state_.index_);
            while (!node->value()) {
                auto i = 0u;
                node = node->child(i);
                state_ = state_t{node->parent(), i};
            }

            return *this;
        }
        const_trie_map_iterator & operator--() noexcept
        {
            // Decrement-from-end case.
            if (state_.index_ == state_.parent_->size()) {
                --state_.index_;
                return *this;
            }

            // Back up one node at a time until we find an ancestor with a
            // value or a previous sibling.
            while (state_.parent_->parent() && state_.index_ == 0) {
                state_ = parent_state(state_);
                if (state_.parent_->child(state_.index_)->value())
                    return *this;
            }

            // If we get found no value above, go down the maximum subtree of
            // the previous sibling.
            if (state_.index_)
                --state_.index_;
            auto node = state_.parent_->child(state_.index_);
            while (!node->empty()) {
                auto i = node->size() - 1;
                node = node->child(i);
                state_ = state_t{node->parent(), i};
            }

            return *this;
        }

        friend bool operator==(
            const_trie_map_iterator lhs, const_trie_map_iterator rhs) noexcept
        {
            return lhs.state_.parent_ == rhs.state_.parent_ &&
                   lhs.state_.index_ == rhs.state_.index_;
        }

        using base_type = stl_interfaces::iterator_interface<
            const_trie_map_iterator<Key, Value>,
            std::bidirectional_iterator_tag,
            trie_element<Key, Value>,
            trie_element<Key, Value const &>>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN

    private:
        explicit const_trie_map_iterator(state_t state) : state_(state) {}

        template<typename KeyT, typename ValueT, typename Compare>
        friend struct trie_map;
        template<typename KeyT, typename Compare>
        friend struct trie_set;
        template<typename KeyT, typename ValueT>
        friend struct trie_map_iterator;
        template<typename KeyT>
        friend struct const_trie_set_iterator;

#endif
    };

    template<typename Key, typename Value>
    struct trie_map_iterator : stl_interfaces::iterator_interface<
                                   trie_map_iterator<Key, Value>,
                                   std::bidirectional_iterator_tag,
                                   trie_element<Key, Value>,
                                   trie_element<Key, Value &>>
    {
    private:
        const_trie_map_iterator<Key, Value> it_;
        using ref_type = trie_element<Key, Value &>;
        using ptr_type = stl_interfaces::proxy_arrow_result<ref_type>;

    public:
        trie_map_iterator() noexcept {}

        trie_map_iterator(trie_match_result match_result) noexcept :
            trie_map_iterator(const_trie_map_iterator<Key, Value>(match_result))
        {}

        ref_type operator*() const
            noexcept(noexcept(detail::reconstruct_key(it_.state_)))
        {
            return ref_type{detail::reconstruct_key(it_.state_),
                            it_.state_.parent_->child_value(it_.state_.index_)};
        };

        ptr_type operator->() const
            noexcept(noexcept(detail::reconstruct_key(it_.state_)))
        {
            ref_type && deref_result = **this;
            return ptr_type(
                ref_type{std::move(deref_result.key), deref_result.value});
        }

        trie_map_iterator & operator++() noexcept
        {
            ++it_;
            return *this;
        }
        trie_map_iterator & operator--() noexcept
        {
            --it_;
            return *this;
        }

        friend bool
        operator==(trie_map_iterator lhs, trie_map_iterator rhs) noexcept
        {
            return lhs.it_ == rhs.it_;
        }

        using base_type = stl_interfaces::iterator_interface<
            trie_map_iterator<Key, Value>,
            std::bidirectional_iterator_tag,
            trie_element<Key, Value>,
            trie_element<Key, Value &>>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN

    private:
        explicit trie_map_iterator(
            detail::trie_iterator_state_t<Key, Value> state) :
            it_(state)
        {}
        explicit trie_map_iterator(const_trie_map_iterator<Key, Value> it) :
            it_(it)
        {}

        template<typename KeyT, typename ValueT, typename Compare>
        friend struct trie_map;
        template<typename KeyT, typename Compare>
        friend struct trie_set;

#endif
    };

}}

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<typename Iter>
    inline constexpr bool enable_borrowed_range<boost::text::trie_range<Iter>> =
        true;
    template<typename Iter>
    inline constexpr bool
        enable_borrowed_range<boost::text::const_trie_range<Iter>> = true;
}

#endif

#endif
