// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/sequence_container_interface.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <tuple>

#include <cassert>


// There's a test that uses this example header; this controls whether the
// C++14 version (v1::) of sequence_container_interface gets used, or the
// C++20 version (v2::).
#if defined(USE_V2)
template<typename Derived, boost::stl_interfaces::element_layout Contiguity>
using sequence_container_interface =
    boost::stl_interfaces::v2::sequence_container_interface<Derived>;
#else
template<typename Derived, boost::stl_interfaces::element_layout Contiguity>
using sequence_container_interface =
    boost::stl_interfaces::v1::sequence_container_interface<Derived, Contiguity>;
#endif


//[ static_vector_defn
// The sections of member functions below are commented as they are in the
// standard for std::vector.  Each section has two numbers: the number of
// member functions in that section, and the number that are missing, because
// they are provided by sequence_container_interface.  The purely
// allocator-specific members are neither present nor part of the counts.
//
// We're passing boost::stl_interfaces::contiguous here, so that
// sequence_container_interface knows that it should provide data().
template<typename T, std::size_t N>
struct static_vector : sequence_container_interface<
                           static_vector<T, N>,
                           boost::stl_interfaces::element_layout::contiguous>
{
    // These are the types required for reversible containers.  These must be
    // user-defined.
    using value_type = T;
    using pointer = T *;
    using const_pointer = T const *;
    using reference = value_type &;
    using const_reference = value_type const &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = T *;
    using const_iterator = T const *;
    using reverse_iterator = boost::stl_interfaces::reverse_iterator<iterator>;
    using const_reverse_iterator =
        boost::stl_interfaces::reverse_iterator<const_iterator>;

    // construct/copy/destroy (9 members, skipped 2)
    //
    // Constructors and special member functions all must be user-provided.
    // Were they provided by sequence_container_interface, everything would
    // break, due to the language rules related to them.  However, assignment
    // from std::initializer_list can come from sequence_container_interface.
    static_vector() noexcept : size_(0) {}
    explicit static_vector(size_type n) : size_(0) { resize(n); }
    explicit static_vector(size_type n, T const & x) : size_(0)
    {
        // Note that you must write "this->" before all the member functions
        // provided by sequence_container_interface, which is slightly annoying.
        this->assign(n, x);
    }
    template<
        typename InputIterator,
        typename Enable = std::enable_if_t<std::is_convertible<
            typename std::iterator_traits<InputIterator>::iterator_category,
            std::input_iterator_tag>::value>>
    static_vector(InputIterator first, InputIterator last) : size_(0)
    {
        this->assign(first, last);
    }
    static_vector(std::initializer_list<T> il) :
        static_vector(il.begin(), il.end())
    {}
    static_vector(static_vector const & other) : size_(0)
    {
        this->assign(other.begin(), other.end());
    }
    static_vector(static_vector && other) noexcept(
        noexcept(std::declval<static_vector>().emplace_back(
            std::move(*other.begin())))) :
        size_(0)
    {
        for (auto & element : other) {
            emplace_back(std::move(element));
        }
        other.clear();
    }
    static_vector & operator=(static_vector const & other)
    {
        this->clear();
        this->assign(other.begin(), other.end());
        return *this;
    }
    static_vector & operator=(static_vector && other) noexcept(noexcept(
        std::declval<static_vector>().emplace_back(std::move(*other.begin()))))
    {
        this->clear();
        for (auto & element : other) {
            emplace_back(std::move(element));
        }
        other.clear();
        return *this;
    }
    ~static_vector() { this->clear(); }

    // iterators (2 members, skipped 10)
    //
    // This section is the first big win.  Instead of having to write 12
    // overloads line begin, cbegin, rbegin, crbegin, etc., we can just write
    // 2.
    iterator begin() noexcept { return reinterpret_cast<T *>(buf_); }
    iterator end() noexcept
    {
        return reinterpret_cast<T *>(buf_ + size_ * sizeof(T));
    }

    // capacity (6 members, skipped 2)
    //
    // Most of these are not even part of the general requirements, because
    // some are specific to std::vector and related types.  However, we do get
    // empty and size from sequence_container_interface.
    size_type max_size() const noexcept { return N; }
    size_type capacity() const noexcept { return N; }
    void resize(size_type sz) noexcept
    {
        resize_impl(sz, [] { return T(); });
    }
    void resize(size_type sz, T const & x) noexcept
    {
        resize_impl(sz, [&]() -> T const & { return x; });
    }
    void reserve(size_type n) noexcept { assert(n < capacity()); }
    void shrink_to_fit() noexcept {}

    // element access (skipped 8)
    // data access (skipped 2)
    //
    // Another big win.  sequence_container_interface provides all of the
    // overloads of operator[], at, front, back, and data.

    // modifiers (5 members, skipped 9)
    //
    // In this section we again get most of the API from
    // sequence_container_interface.

    // emplace_back does not look very necessary -- just look at its trivial
    // implementation -- but we can't provide it from
    // sequence_container_interface, because it is an optional sequence
    // container interface.  We would not want emplace_front to suddenly
    // appear on our std::vector-like type, and there may be some other type
    // for which emplace_back is a bad idea.
    //
    // However, by providing emplace_back here, we signal to the
    // sequence_container_interface template that our container is
    // back-mutation-friendly, and this allows it to provide all the overloads
    // of push_back and pop_back.
    template<typename... Args>
    reference emplace_back(Args &&... args)
    {
        return *emplace(end(), std::forward<Args>(args)...);
    }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args &&... args)
    {
        auto position = const_cast<T *>(pos);
        bool const insert_before_end = position < end();
        if (insert_before_end) {
            auto last = end();
            emplace_back(std::move(this->back()));
            std::move_backward(position, last - 1, last);
        }
        new (position) T(std::forward<Args>(args)...);
        if (!insert_before_end)
            ++size_;
        return position;
    }
    // Note: The iterator category here was upgraded to ForwardIterator
    // (instead of vector's InputIterator), to ensure linear time complexity.
    template<
        typename ForwardIterator,
        typename Enable = std::enable_if_t<std::is_convertible<
            typename std::iterator_traits<ForwardIterator>::iterator_category,
            std::forward_iterator_tag>::value>>
    iterator
    insert(const_iterator pos, ForwardIterator first, ForwardIterator last)
    {
        auto position = const_cast<T *>(pos);
        auto const insertions = std::distance(first, last);
        assert(this->size() + insertions < capacity());
        uninitialized_generate(end(), end() + insertions, [] { return T(); });
        std::move_backward(position, end(), end() + insertions);
        std::copy(first, last, position);
        size_ += insertions;
        return position;
    }
    iterator erase(const_iterator f, const_iterator l)
    {
        auto first = const_cast<T *>(f);
        auto last = const_cast<T *>(l);
        auto end_ = this->end();
        auto it = std::move(last, end_, first);
        for (; it != end_; ++it) {
            it->~T();
        }
        size_ -= last - first;
        return first;
    }
    void swap(static_vector & other)
    {
        size_type short_size, long_size;
        std::tie(short_size, long_size) =
            std::minmax(this->size(), other.size());
        for (auto i = size_type(0); i < short_size; ++i) {
            using std::swap;
            swap((*this)[i], other[i]);
        }

        static_vector * longer = this;
        static_vector * shorter = this;
        if (this->size() < other.size())
            longer = &other;
        else
            shorter = &other;

        for (auto it = longer->begin() + short_size, last = longer->end();
             it != last;
             ++it) {
            shorter->emplace_back(std::move(*it));
        }

        longer->resize(short_size);
        shorter->size_ = long_size;
    }

    // Since we're getting so many overloads from
    // sequence_container_interface, and since many of those overloads are
    // implemented in terms of a user-defined function of the same name, we
    // need to add quite a few using declarations here.
    using base_type = sequence_container_interface<
        static_vector<T, N>,
        boost::stl_interfaces::element_layout::contiguous>;
    using base_type::begin;
    using base_type::end;
    using base_type::insert;
    using base_type::erase;

    // comparisons (skipped 6)

private:
    template<typename F>
    static void uninitialized_generate(iterator f, iterator l, F func)
    {
        for (; f != l; ++f) {
            new (static_cast<void *>(std::addressof(*f))) T(func());
        }
    }
    template<typename F>
    void resize_impl(size_type sz, F func) noexcept
    {
        assert(sz < capacity());
        if (sz < this->size())
            erase(begin() + sz, end());
        if (this->size() < sz)
            uninitialized_generate(end(), begin() + sz, func);
        size_ = sz;
    }

    alignas(T) unsigned char buf_[N * sizeof(T)];
    size_type size_;
};
//]
