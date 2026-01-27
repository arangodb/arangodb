// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_TRIE_HPP
#define BOOST_TEXT_TRIE_HPP

#include <boost/optional.hpp>
#include <boost/text/trie_fwd.hpp>
#include <boost/algorithm/cxx14/equal.hpp>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>


namespace boost { namespace text {

    /** An optional reference.  Its optionality is testable, via the operator
        bool() members, and it is implicitly convertible to the underlying
        value type. */
    template<typename T, bool Const = std::is_const<T>::value>
    struct optional_ref
    {
    private:
        T * t_;

    public:
        optional_ref() : t_(nullptr) {}
        optional_ref(T & t) : t_(&t) {}

        template<typename U>
        auto operator=(U && u)
            -> decltype(*this->t_ = static_cast<U &&>(u), *this)
        {
            BOOST_ASSERT(t_);
            *t_ = static_cast<U &&>(u);
            return *this;
        }

        explicit operator bool() const & noexcept { return t_ != nullptr; }
        explicit operator bool() & noexcept { return t_ != nullptr; }
        explicit operator bool() && noexcept { return t_ != nullptr; }

        T const & operator*() const noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }
        T const * operator->() const noexcept
        {
            BOOST_ASSERT(t_);
            return t_;
        }

        operator T const &() const & noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }

        operator T const &() const && noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }

        T & operator*() noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }

        T * operator->() noexcept
        {
            BOOST_ASSERT(t_);
            return t_;
        }

        operator T &() & noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }

        operator T &() && noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }
    };

    // TODO: This specialization is inexplicably assignable.
    template<typename T>
    struct optional_ref<T, true>
    {
    private:
        T * t_;

    public:
        optional_ref() : t_(nullptr) {}
        optional_ref(T & t) : t_(&t) {}

        explicit operator bool() const & noexcept { return t_ != nullptr; }
        explicit operator bool() && noexcept { return t_ != nullptr; }

        T & operator*() const noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }
        T * operator->() const noexcept
        {
            BOOST_ASSERT(t_);
            return t_;
        }

        operator T &() const & noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }

        operator T &() const && noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }
    };

    template<>
    struct optional_ref<bool, false>
    {
    private:
        bool * t_;

    public:
        optional_ref() : t_(nullptr) {}
        optional_ref(bool & t) : t_(&t) {}

        template<typename U>
        auto operator=(U && u)
            -> decltype(*this->t_ = static_cast<U &&>(u), *this)
        {
            BOOST_ASSERT(t_);
            *t_ = static_cast<U &&>(u);
            return *this;
        }

        explicit operator bool() const & noexcept { return t_ != nullptr; }
        explicit operator bool() && noexcept { return t_ != nullptr; }

        bool const & operator*() const noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }
        bool const * operator->() const noexcept
        {
            BOOST_ASSERT(t_);
            return t_;
        }

        bool & operator*() noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }

        bool * operator->() noexcept
        {
            BOOST_ASSERT(t_);
            return t_;
        }
    };

    template<>
    struct optional_ref<bool const, true>
    {
    private:
        bool const * t_;

    public:
        optional_ref() : t_(nullptr) {}
        optional_ref(bool const & t) : t_(&t) {}

        explicit operator bool() const & noexcept { return t_ != nullptr; }
        explicit operator bool() && noexcept { return t_ != nullptr; }

        bool const & operator*() const noexcept
        {
            BOOST_ASSERT(t_);
            return *t_;
        }
        bool const * operator->() const noexcept
        {
            BOOST_ASSERT(t_);
            return t_;
        }
    };

    namespace detail {

        template<
            typename ParentIndexing,
            typename Key,
            typename Value,
            std::size_t KeySize = 0>
        struct trie_node_t;

        struct no_index_within_parent_t
        {
            std::size_t value() const noexcept
            {
                BOOST_ASSERT(!"This should never be called.");
                return 0;
            }

            template<
                typename Key,
                typename Value,
                typename Iter,
                std::size_t KeySize>
            void insert_at(
                std::unique_ptr<trie_node_t<
                    no_index_within_parent_t,
                    Key,
                    Value,
                    KeySize>> const & child,
                std::ptrdiff_t offset,
                Iter it,
                Iter end) noexcept
            {}

            template<typename Key, typename Value, std::size_t KeySize>
            void insert_ptr(std::unique_ptr<trie_node_t<
                                no_index_within_parent_t,
                                Key,
                                Value,
                                KeySize>> const & child) noexcept
            {}

            template<typename Iter>
            void erase(Iter it, Iter end) noexcept
            {}
        };

        template<typename Char>
        struct char_range
        {
            Char * first_;
            Char * last_;

            Char * begin() const noexcept { return first_; }
            Char * end() const noexcept { return last_; }
        };

        struct void_type
        {};
    }

    /** A key/value pair found in a trie or trie_map. */
    template<typename Key, typename Value>
    struct trie_element
    {
        trie_element() {}
        trie_element(Key k, Value v) : key(k), value(v) {}

        template<typename KeyT, typename ValueT>
        trie_element(trie_element<KeyT, ValueT> const & rhs) :
            key(rhs.key),
            value(rhs.value)
        {}

        template<typename KeyT, typename ValueT>
        trie_element(trie_element<KeyT, ValueT> && rhs) :
            key(std::move(rhs.key)),
            value(std::move(rhs.value))
        {}

        template<typename KeyT, typename ValueT>
        trie_element & operator=(trie_element<KeyT, ValueT> const & rhs)
        {
            key = rhs.key;
            value = rhs.value;
            return *this;
        }

        template<typename KeyT, typename ValueT>
        trie_element & operator=(trie_element<KeyT, ValueT> && rhs)
        {
            key = std::move(rhs.key);
            value = std::move(rhs.value);
            return *this;
        }

        Key key;
        Value value;

        friend bool
        operator==(trie_element const & lhs, trie_element const & rhs)
        {
            return lhs.key == rhs.key && lhs.value == rhs.value;
        }
        friend bool
        operator!=(trie_element const & lhs, trie_element const & rhs)
        {
            return !(lhs == rhs);
        }
    };

    /** The result type for trie operations that produce a matching
        subsequence. */
    struct trie_match_result
    {
        trie_match_result() : node(nullptr), size(0), match(false), leaf(false)
        {}
        trie_match_result(void const * n, std::ptrdiff_t s, bool m, bool l) :
            node(n),
            size(s),
            match(m),
            leaf(l)
        {}

        /** An opaque pointer to the underlying trie node. */
        void const * node;

        /** The size/length of the match, from the root through `node`,
            inclusive. */
        std::ptrdiff_t size;

        /** True iff this result represents a match.  Stated another way,
            `match` is true iff `node` represents an element in the trie
            (whether or not `node` has children). */
        bool match;

        /** True iff `node` is a leaf (that is, that `node` hs no
            children). */
        bool leaf;

        friend bool
        operator==(trie_match_result const & lhs, trie_match_result const & rhs)
        {
            return lhs.node == rhs.node && lhs.size == rhs.size &&
                   lhs.match == rhs.match && lhs.leaf == rhs.leaf;
        }
        friend bool
        operator!=(trie_match_result const & lhs, trie_match_result const & rhs)
        {
            return !(lhs == rhs);
        }
    };

    // TODO: KeyIter, KeyRange, and Range concepts.
    // TODO: Key concept specifying that Key is a container.
    // TODO: Compare concept specifying that Compare compares Key::value_types.
    // Don't forget to mention that Compare must be statically polymorphic.

    /** A trie, or prefix-tree.  A trie contains a set of keys, each of which
        is a sequence, and a set of values, each associated with some key.
        Each node in the trie represents some prefix found in at least one
        member of the set of values contained in the trie.  If a certain node
        represents the end of one of the keys, it has an associated value.
        Such a node may or may not have children.

        Complexity of lookups is always O(M), where M is the size of the Key
        being lookep up.  Note that this implies that lookup complexity is
        independent of the size of the trie.

        \param Key The key-type; must be a sequence of values comparable via
        Compare()(x, y).
        \param Value The value-type.
        \param Compare The type of the comparison object used to compare
        elements of the key-type.

        \note trie is not a C++ Container, according to the definition in the
        C++ standard, in part because it does not have any iterators.  This
        allows trie to do slightly less work when it does insertions and
        erasures.  If you need iterators, use trie_map or trie_set. */
    template<
        typename Key,
        typename Value,
        typename Compare,
        std::size_t KeySize>
    struct trie
    {
    private:
        using node_t = detail::
            trie_node_t<detail::no_index_within_parent_t, Key, Value, KeySize>;

    public:
        using key_type = Key;
        using value_type = Value;
        using key_compare = Compare;
        using key_element_type = typename Key::value_type;

        using reference = value_type &;
        using const_reference = value_type const &;
        using size_type = std::ptrdiff_t;
        using difference_type = std::ptrdiff_t;

        using match_result = trie_match_result;

        trie() : size_(0) {}

        trie(Compare const & comp) : size_(0), comp_(comp) {}

        template<typename Iter, typename Sentinel>
        trie(Iter first, Sentinel last, Compare const & comp = Compare()) :
            size_(0),
            comp_(comp)
        {
            insert(first, last);
        }
        template<typename Range>
        explicit trie(Range r, Compare const & comp = Compare()) :
            size_(0),
            comp_(comp)
        {
            insert(std::begin(r), std::end(r));
        }
        trie(std::initializer_list<trie_element<key_type, value_type>> il) :
            size_(0)
        {
            insert(il);
        }

        trie &
        operator=(std::initializer_list<trie_element<key_type, value_type>> il)
        {
            clear();
            for (auto const & x : il) {
                insert(x);
            }
            return *this;
        }

        bool empty() const noexcept { return size_ == 0; }
        size_type size() const noexcept { return size_; }

#ifndef BOOST_TEXT_DOXYGEN

#define BOOST_TRIE_C_STR_OVERLOAD(rtype, func, quals)                          \
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
            auto first = std::begin(key);
            auto const last = std::end(key);
            auto match = longest_match_impl<false>(first, last);
            return first == last && match.match;
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_C_STR_OVERLOAD(bool, contains, const noexcept)
#endif

        /** Returns the longest subsequence of `[first, last)` found in *this,
            whether or not it is a match. */
        template<typename KeyIter, typename Sentinel>
        match_result longest_subsequence(KeyIter first, Sentinel last) const
            noexcept
        {
            return longest_match_impl<false>(first, last);
        }

        /** Returns the longest subsequence of `key` found in *this, whether
           or not it is a match. */
        template<typename KeyRange>
        match_result longest_subsequence(KeyRange const & key) const noexcept
        {
            return longest_subsequence(std::begin(key), std::end(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_C_STR_OVERLOAD(
            match_result, longest_subsequence, const noexcept)
#endif

        /** Returns the longest matching subsequence of `[first, last)` found
            in *this. */
        template<typename KeyIter, typename Sentinel>
        match_result longest_match(KeyIter first, Sentinel last) const noexcept
        {
            return longest_match_impl<true>(first, last);
        }

        /** Returns the longest matching subsequence of `key` found in
            *this. */
        template<typename KeyRange>
        match_result longest_match(KeyRange const & key) const noexcept
        {
            return longest_match(std::begin(key), std::end(key));
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_C_STR_OVERLOAD(match_result, longest_match, const noexcept)
#endif

        /** Returns the result of extending `prev` by one element, `e`. */
        template<typename KeyElementT>
        match_result extend_subsequence(match_result prev, KeyElementT e) const
            noexcept
        {
            auto e_ptr = &e;
            return extend_subsequence_impl<false>(
                match_result{prev}, e_ptr, e_ptr + 1);
        }

        /** Returns the result of extending `prev` by the longest subsequence
            of `[first, last)` found in *this. */
        template<typename KeyIter, typename Sentinel>
        match_result extend_subsequence(
            match_result prev, KeyIter first, Sentinel last) const noexcept
        {
            return extend_subsequence_impl<false>(match_result{prev}, first, last);
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
            return extend_subsequence_impl<true>(prev, e_ptr, e_ptr + 1);
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
            return extend_subsequence_impl<true>(prev, first, last);
        }

        /** Writes the sequence of elements that would advance `prev` by one
            element to `out`, and returns the final value of `out` after the
            writes. */
        template<typename OutIter>
        OutIter copy_next_key_elements(match_result prev, OutIter out) const
        {
            auto node = to_node_ptr(prev.node);
            return node->copy_next_key_elements(out);
        }

        /** Returns an optional reference to the const value associated with
            `key` in *this (if any). */
        template<typename KeyRange>
        optional_ref<value_type const> operator[](KeyRange const & key) const
            noexcept
        {
            auto first = std::begin(key);
            auto const last = std::end(key);
            auto match = longest_match_impl<false>(first, last);
            if (first != last || !match.match)
                return {};
            return *to_node_ptr(match.node)->value();
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_C_STR_OVERLOAD(
            optional_ref<value_type const>, operator[], const noexcept)
#endif

        /** Returns an optional reference to the const value associated with
            `match` in *this (if any). */
        optional_ref<value_type const> operator[](match_result match) const
            noexcept
        {
            if (!match.match)
                return {};
            return *to_node_ptr(match.node)->value();
        }

        void clear() noexcept
        {
            header_ = node_t();
            size_ = 0;
        }

        /** Returns an optional reference to the value associated with `key`
           in *this (if any). */
        template<typename KeyRange>
        optional_ref<value_type> operator[](KeyRange const & key) noexcept
        {
            auto first = std::begin(key);
            auto const last = std::end(key);
            auto match = longest_match_impl<false>(first, last);
            if (first != last || !match.match)
                return {};
            return *const_cast<node_t *>(to_node_ptr(match.node))->value();
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_C_STR_OVERLOAD(
            optional_ref<value_type>, operator[], noexcept)
#endif

        /** Returns an optional reference to the value associated with `match`
            in *this (if any). */
        optional_ref<value_type> operator[](match_result match) noexcept
        {
            if (!match.match)
                return {};
            return *const_cast<node_t *>(to_node_ptr(match.node))->value();
        }

        /** Inserts the key/value pair `[first, last)`, `value` into *this.
            Returns false if the key already exists in *this, true
            otherwise. */
        template<typename KeyIter, typename Sentinel>
        bool insert(KeyIter first, Sentinel last, Value value)
        {
            if (empty()) {
                std::unique_ptr<node_t> new_node(new node_t(&header_));
                header_.insert(std::move(new_node));
            }

            auto match = longest_match_impl<false>(first, last);
            if (first == last && match.match)
                return false;

            auto node = create_children(
                const_cast<node_t *>(to_node_ptr(match.node)), first, last);
            node->value() = std::move(value);
            ++size_;

            return true;
        }

        /** Inserts the key/value pair `key`, `value` into *this.  Returns
            false if the key already exists in *this, true otherwise. */
        template<typename KeyRange>
        bool insert(KeyRange const & key, Value value)
        {
            return insert(std::begin(key), std::end(key), std::move(value));
        }

        template<typename Char, std::size_t N>
        bool insert(Char const (&chars)[N], Value value)
        {
            static_assert(
                std::is_same<Char, key_element_type>::value,
                "Only well-formed when Char is Key::value_type.");
            return insert(
                detail::char_range<Char const>{chars, chars + N - 1},
                std::move(value));
        }

        /** Inserts the sequence of key/value pairs `[first, last)` into
            *this. */
        template<typename Iter, typename Sentinel>
        void insert(Iter first, Sentinel last)
        {
            for (; first != last; ++first) {
                insert(first->key, first->value);
            }
        }

        /** Inserts the element e.key, e.value into *this. */
        bool insert(trie_element<key_type, value_type> const & e)
        {
            return insert(e.key, e.value);
        }

        /** Inserts the sequence of key/value pairs `r` into
         *this. */
        template<typename Range>
        bool insert(Range const & r)
        {
            return insert(std::begin(r), std::end(r));
        }

        /** Inserts the sequence of key/value pairs `il` into *this. */
        void
        insert(std::initializer_list<trie_element<key_type, value_type>> il)
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
        void insert_or_assign(KeyIter first, Sentinel last, Value value)
        {
            auto it = first;
            auto match = longest_match_impl<false>(it, last);
            if (it == last && match.match) {
                const_cast<Value &>(*to_node_ptr(match.node)->value()) =
                    std::move(value);
            }
            insert(first, last, std::move(value));
        }

        /** Inserts the key/value pair `key`, `value` into *this, or assigns
            `value` over the existing value associated with `key`, if `key` is
            already found in *this.  The `inserted` field of the result will
            be true if the operation resulted in a new insertion, or false
            otherwise. */
        template<typename KeyRange>
        void insert_or_assign(KeyRange const & key, Value value)
        {
            insert_or_assign(std::begin(key), std::end(key), std::move(value));
        }

        /** Erases the key/value pair associated with `key` from *this.
            Returns true if the key is found in *this, false otherwise. */
        template<typename KeyRange>
        bool erase(KeyRange const & key)
        {
            auto first = std::begin(key);
            auto const last = std::end(key);
            auto match = longest_match_impl<false>(first, last);
            if (first != last || !match.match)
                return false;
            return erase(match);
        }

        /** Erases the key/value pair associated with `match` from *this.
            Returns true if the key is found in *this, false otherwise. */
        bool erase(match_result match) noexcept
        {
            auto node = const_cast<node_t *>(to_node_ptr(match.node));
            if (node == &header_)
                return false;

            --size_;

            if (!node->empty()) {
                // node has a value, but also children.  Remove the value and
                // return the next-iterator.
                node->value() = optional<Value>();
                return true;
            }

            // node has a value, *and* no children.  Remove it and all its
            // singular predecessors.
            auto parent = const_cast<node_t *>(node->parent());
            parent->erase(node);
            while (parent->parent() && parent->empty() && !parent->value()) {
                node = parent;
                parent = const_cast<node_t *>(node->parent());
                parent->erase(node);
            }

            return true;
        }

#ifndef BOOST_TEXT_DOXYGEN
        BOOST_TRIE_C_STR_OVERLOAD(bool, erase, noexcept)
#endif

        void swap(trie & other)
        {
            header_.swap(other.header_);
            std::swap(size_, other.size_);
            std::swap(comp_, other.comp_);
        }

        friend bool operator==(trie const & lhs, trie const & rhs)
        {
            if (lhs.size_ != rhs.size_)
                return false;
            return lhs.header_ == rhs.header_;
        }
        friend bool operator!=(trie const & lhs, trie const & rhs)
        {
            return !(lhs == rhs);
        }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        trie const * const_this() { return const_cast<trie const *>(this); }
        static node_t const * to_node_ptr(void const * ptr)
        {
            return static_cast<node_t const *>(ptr);
        }

        template<bool StopAtMatch, typename KeyIter, typename Sentinel>
        match_result longest_match_impl(
            KeyIter & first, Sentinel last) const noexcept
        {
            return extend_subsequence_impl<StopAtMatch>(
                match_result{&header_, 0, false, true}, first, last);
        }

        template<bool StopAtMatch, typename KeyIter, typename Sentinel>
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
            node_t const * last_match_node = nullptr;
            size_type last_match_size = 0;
            while (first != last) {
                auto const it = node->find(*first, comp_);
                if (it == node->end())
                    break;
                ++first;
                ++size;
                node = it->get();
                if (StopAtMatch && !!node->value()) {
                    last_match_node = node;
                    last_match_size = size;
                }
            }
            if (StopAtMatch && last_match_node) {
                node = last_match_node;
                size = last_match_size;
            }

            return match_result{node, size, !!node->value(), node->empty()};
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

        node_t header_;
        size_type size_;
        key_compare comp_;

        template<typename Key2, typename Value2, typename Compare2>
        friend struct trie_map;
#endif
    };

    namespace detail {

        template<
            typename ParentIndexing,
            typename Key,
            typename Value,
            std::size_t KeySize>
        struct trie_node_t
        {
            using children_t = std::vector<std::unique_ptr<trie_node_t>>;
            using iterator = typename children_t::iterator;
            using const_iterator = typename children_t::const_iterator;
            using key_element = typename Key::value_type;

            static_assert(std::is_unsigned<key_element>::value, "");

            trie_node_t() : parent_(nullptr) {}
            trie_node_t(trie_node_t * parent) : parent_(parent) {}
            trie_node_t(trie_node_t const & other) :
                children_(other.children_.size()),
                value_(other.value_),
                parent_(other.parent_),
                index_within_parent_(other.index_within_parent_)
            {
                for (int i = 0, end = (int)children_.size(); i < end; ++i) {
                    if (other.children_[i]) {
                        children_[i].reset(
                            new trie_node_t(*other.children_[i]));
                    }
                }
            }
            trie_node_t(trie_node_t && other) : parent_(nullptr)
            {
                swap(other);
            }
            trie_node_t & operator=(trie_node_t const & rhs)
            {
                BOOST_ASSERT(
                    parent_ == nullptr &&
                    "Assignment of trie_node_ts are defined only for the "
                    "header node.");
                trie_node_t temp(rhs);
                temp.swap(*this);
                return *this;
            }
            trie_node_t & operator=(trie_node_t && rhs)
            {
                BOOST_ASSERT(
                    parent_ == nullptr &&
                    "Move assignments of trie_node_ts are defined only for the "
                    "header node.");
                trie_node_t temp(std::move(rhs));
                temp.swap(*this);
                return *this;
            }

            auto max_size() const noexcept { return KeySize; }

            optional<Value> const & value() const noexcept { return value_; }

            Value & child_value(std::size_t i) const
            {
                return *children_[i]->value_;
            }

            trie_node_t * parent() const noexcept { return parent_; }

            bool empty() const noexcept { return children_.size() == 0; }

            const_iterator end() const noexcept { return children_.end(); }

            std::size_t index_within_parent() const noexcept
            {
                return index_within_parent_.value();
            }

            bool before_child_subtree(key_element const & e) const noexcept
            {
                return e < key_element(0);
            }

            template<typename Compare>
            const_iterator
            lower_bound(key_element const & e, Compare const &) const noexcept
            {
                return children_.empty() ? children_.end() : children_.begin() + e;
            }
            template<typename Compare>
            const_iterator
            find(key_element const & e, Compare const & comp) const noexcept
            {
                auto const it = lower_bound(e, comp);
                if (children_.empty() || !*it)
                    return children_.end();
                return it;
            }

            template<typename Compare>
            trie_node_t const *
            child(key_element const & e, Compare const &) const noexcept
            {
                return children_[e].get();
            }
            trie_node_t const * child(std::size_t i) const noexcept
            {
                return children_[i].get();
            }

            key_element const & key(std::size_t i) const noexcept
            {
                BOOST_ASSERT(key_element(i) == i);
                return key_element(i);
            }

            template<typename OutIter>
            OutIter copy_next_key_elements(OutIter out) const
            {
                for (key_element i = 0, end = (key_element)children_.size();
                     i < end;
                     ++i) {
                    if (children_[i]) {
                        *out = i;
                        ++out;
                    }
                }
                return out;
            }

            void swap(trie_node_t & other)
            {
                BOOST_ASSERT(
                    parent_ == nullptr &&
                    "Swaps of trie_node_ts are defined only for the header "
                    "node.");
                children_.swap(other.children_);
                value_.swap(other.value_);
                for (auto const & node : children_) {
                    if (node)
                        node->parent_ = this;
                }
                for (auto const & node : other.children_) {
                    if (node)
                        node->parent_ = &other;
                }
                std::swap(index_within_parent_, other.index_within_parent_);
            }

            optional<Value> & value() noexcept { return value_; }

            template<typename Compare>
            iterator insert(
                key_element const & e,
                Compare const & comp,
                std::unique_ptr<trie_node_t> && child)
            {
                if (children_.empty())
                    children_.resize(max_size());
                auto child_it = children_.begin() + e;
                index_within_parent_.insert_at(
                    child, e, child_it, children_.end());
                children_[e] = std::move(child);
                return child_it;
            }
            iterator insert(std::unique_ptr<trie_node_t> && child)
            {
                if (children_.empty())
                    children_.resize(max_size());
                index_within_parent_.insert_ptr(child);
                return children_.insert(children_.begin(), std::move(child));
            }
            void erase(std::size_t i) noexcept
            {
                auto it = children_.begin() + i;
                it->reset(nullptr);
                index_within_parent_.erase(it, children_.end());
            }
            void erase(trie_node_t const * child) noexcept
            {
                auto const it = std::find_if(
                    children_.begin(),
                    children_.end(),
                    [child](std::unique_ptr<trie_node_t> const & ptr) {
                        return child == ptr.get();
                    });
                BOOST_ASSERT(it != children_.end());
                erase(it - children_.begin());
            }

            template<typename Compare>
            iterator
            lower_bound(key_element const & e, Compare const &) noexcept
            {
                return children_.begin() + e;
            }
            template<typename Compare>
            iterator find(key_element const & e, Compare const & comp) noexcept
            {
                if (children_.empty())
                    return children_.end();
                auto const it = lower_bound(e, comp);
                if (!*it)
                    return children_.end();
                return it;
            }

            template<typename Compare>
            trie_node_t * child(key_element const & e, Compare const &) noexcept
            {
                return children_[e].get();
            }
            trie_node_t * child(std::size_t i) noexcept
            {
                return children_[i].get();
            }

            friend bool
            operator==(trie_node_t const & lhs, trie_node_t const & rhs)
            {
                if (lhs.value_ != rhs.value_)
                    return false;
                return algorithm::equal(
                    lhs.children_.begin(),
                    lhs.children_.end(),
                    rhs.children_.begin(),
                    rhs.children_.end(),
                    [](auto const & l_ptr, auto const & r_ptr) {
                        if (!l_ptr && !r_ptr)
                            return true;
                        if (!l_ptr || !r_ptr)
                            return false;
                        return *l_ptr == *r_ptr;
                    });
            }
            friend bool
            operator!=(trie_node_t const & lhs, trie_node_t const & rhs)
            {
                return !(lhs == rhs);
            }

        private:
            trie_node_t const * const_this()
            {
                return const_cast<trie_node_t const *>(this);
            }
            key_element const & key(const_iterator it) const
            {
                return key_element(it - children_.begin());
            }

            children_t children_;
            optional<Value> value_;
            trie_node_t * parent_;
            ParentIndexing index_within_parent_;

            friend struct index_within_parent_t;
        };

        template<typename ParentIndexing, typename Key, typename Value>
        struct trie_node_t<ParentIndexing, Key, Value, 0>
        {
            using children_t = std::vector<std::unique_ptr<trie_node_t>>;
            using iterator = typename children_t::iterator;
            using const_iterator = typename children_t::const_iterator;
            using key_element = typename Key::value_type;
            using keys_t = std::vector<key_element>;
            using key_iterator = typename keys_t::const_iterator;

            trie_node_t() : parent_(nullptr) {}
            trie_node_t(trie_node_t * parent) : parent_(parent) {}
            trie_node_t(trie_node_t const & other) :
                keys_(other.keys_),
                value_(other.value_),
                parent_(other.parent_),
                index_within_parent_(other.index_within_parent_)
            {
                children_.reserve(other.children_.size());
                for (auto const & node : other.children_) {
                    std::unique_ptr<trie_node_t> new_node(
                        new trie_node_t(*node));
                    children_.push_back(std::move(new_node));
                }
            }
            trie_node_t(trie_node_t && other) : parent_(nullptr)
            {
                swap(other);
            }
            trie_node_t & operator=(trie_node_t const & rhs)
            {
                BOOST_ASSERT(
                    parent_ == nullptr &&
                    "Assignment of trie_node_ts are defined only for the "
                    "header node.");
                trie_node_t temp(rhs);
                temp.swap(*this);
                return *this;
            }
            trie_node_t & operator=(trie_node_t && rhs)
            {
                BOOST_ASSERT(
                    parent_ == nullptr &&
                    "Move assignments of trie_node_ts are defined only for the "
                    "header node.");
                trie_node_t temp(std::move(rhs));
                temp.swap(*this);
                return *this;
            }

            optional<Value> const & value() const noexcept { return value_; }

            Value & child_value(std::size_t i) const
            {
                return *children_[i]->value_;
            }

            trie_node_t * parent() const noexcept { return parent_; }
            trie_node_t * min_child() const noexcept
            {
                return children_.front().get();
            }
            trie_node_t * max_child() const noexcept
            {
                return children_.back().get();
            }

            bool empty() const noexcept { return children_.size() == 0; }
            std::size_t size() const noexcept { return children_.size(); }

            bool min_value() const noexcept
            {
                return !!children_.front()->value_;
            }
            bool max_value() const noexcept
            {
                return !!children_.back()->value_;
            }

            const_iterator begin() const noexcept { return children_.begin(); }
            const_iterator end() const noexcept { return children_.end(); }

            key_iterator key_begin() const noexcept { return keys_.begin(); }
            key_iterator key_end() const noexcept { return keys_.end(); }

            std::size_t index_within_parent() const noexcept
            {
                return index_within_parent_.value();
            }

            bool before_child_subtree(key_element const & e) const noexcept
            {
                return keys_.empty() || e < keys_.front();
            }

            template<typename Compare>
            const_iterator
            lower_bound(key_element const & e, Compare const & comp) const
                noexcept
            {
                auto const it =
                    std::lower_bound(keys_.begin(), keys_.end(), e, comp);
                return children_.begin() + (it - keys_.begin());
            }
            template<typename Compare>
            const_iterator
            find(key_element const & e, Compare const & comp) const noexcept
            {
                auto const it = lower_bound(e, comp);
                auto const end_ = end();
                if (it != end_ && comp(e, key(it)))
                    return end_;
                return it;
            }

            template<typename Compare>
            trie_node_t const *
            child(key_element const & e, Compare const & comp) const noexcept
            {
                auto const it = find(e, comp);
                if (it == children_.end())
                    return nullptr;
                return it->get();
            }
            trie_node_t const * child(std::size_t i) const noexcept
            {
                return children_[i].get();
            }

            key_element const & key(std::size_t i) const noexcept
            {
                return keys_[i];
            }

            template<typename OutIter>
            OutIter copy_next_key_elements(OutIter out) const
            {
                return std::copy(key_begin(), key_end(), out);
            }

            void swap(trie_node_t & other)
            {
                BOOST_ASSERT(
                    parent_ == nullptr &&
                    "Swaps of trie_node_ts are defined only for the header "
                    "node.");
                keys_.swap(other.keys_);
                children_.swap(other.children_);
                value_.swap(other.value_);
                for (auto const & node : children_) {
                    node->parent_ = this;
                }
                for (auto const & node : other.children_) {
                    node->parent_ = &other;
                }
                std::swap(index_within_parent_, other.index_within_parent_);
            }

            optional<Value> & value() noexcept { return value_; }

            iterator begin() noexcept { return children_.begin(); }
            iterator end() noexcept { return children_.end(); }

            template<typename Compare>
            iterator insert(
                key_element const & e,
                Compare const & comp,
                std::unique_ptr<trie_node_t> && child)
            {
                BOOST_ASSERT(child->empty());
                auto it = std::lower_bound(keys_.begin(), keys_.end(), e, comp);
                it = keys_.insert(it, e);
                auto const offset = it - keys_.begin();
                auto child_it = children_.begin() + offset;
                index_within_parent_.insert_at(
                    child, offset, child_it, children_.end());
                return children_.insert(child_it, std::move(child));
            }
            iterator insert(std::unique_ptr<trie_node_t> && child)
            {
                BOOST_ASSERT(empty());
                index_within_parent_.insert_ptr(child);
                return children_.insert(children_.begin(), std::move(child));
            }
            void erase(std::size_t i) noexcept
            {
                // This empty-keys situation happens only in the header node.
                if (!keys_.empty())
                    keys_.erase(keys_.begin() + i);
                auto it = children_.erase(children_.begin() + i);
                index_within_parent_.erase(it, children_.end());
            }
            void erase(trie_node_t const * child) noexcept
            {
                auto const it = std::find_if(
                    children_.begin(),
                    children_.end(),
                    [child](std::unique_ptr<trie_node_t> const & ptr) {
                        return child == ptr.get();
                    });
                BOOST_ASSERT(it != children_.end());
                erase(it - children_.begin());
            }

            template<typename Compare>
            iterator
            lower_bound(key_element const & e, Compare const & comp) noexcept
            {
                auto const it = const_this()->lower_bound(e, comp);
                return children_.begin() +
                       (it - const_iterator(children_.begin()));
            }
            template<typename Compare>
            iterator find(key_element const & e, Compare const & comp) noexcept
            {
                auto const it = const_this()->find(e, comp);
                return children_.begin() +
                       (it - const_iterator(children_.begin()));
            }

            template<typename Compare>
            trie_node_t *
            child(key_element const & e, Compare const & comp) noexcept
            {
                return const_cast<trie_node_t *>(const_this()->child(e, comp));
            }
            trie_node_t * child(std::size_t i) noexcept
            {
                return const_cast<trie_node_t *>(const_this()->child(i));
            }

            friend bool
            operator==(trie_node_t const & lhs, trie_node_t const & rhs)
            {
                if (lhs.keys_ != rhs.keys_ || lhs.value_ != rhs.value_)
                    return false;
                return algorithm::equal(
                    lhs.children_.begin(),
                    lhs.children_.end(),
                    rhs.children_.begin(),
                    rhs.children_.end(),
                    [](auto const & l_ptr, auto const & r_ptr) {
                        if (!l_ptr && !r_ptr)
                            return true;
                        if (!l_ptr || !r_ptr)
                            return false;
                        return *l_ptr == *r_ptr;
                    });
            }
            friend bool
            operator!=(trie_node_t const & lhs, trie_node_t const & rhs)
            {
                return !(lhs == rhs);
            }

        private:
            trie_node_t const * const_this()
            {
                return const_cast<trie_node_t const *>(this);
            }
            key_element const & key(const_iterator it) const
            {
                return keys_[it - children_.begin()];
            }

            keys_t keys_;
            children_t children_;
            optional<Value> value_;
            trie_node_t * parent_;
            ParentIndexing index_within_parent_;

            friend struct index_within_parent_t;
        };
    }

}}

#endif
