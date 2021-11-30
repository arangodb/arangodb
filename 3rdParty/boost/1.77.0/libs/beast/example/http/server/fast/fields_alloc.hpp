//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_FIELDS_ALLOC_HPP
#define BOOST_BEAST_EXAMPLE_FIELDS_ALLOC_HPP

#include <boost/throw_exception.hpp>
#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace detail {

struct static_pool
{
    std::size_t size_;
    std::size_t refs_ = 1;
    std::size_t count_ = 0;
    char* p_;

    char*
    end()
    {
        return reinterpret_cast<char*>(this + 1) + size_;
    }

    explicit
    static_pool(std::size_t size)
        : size_(size)
        , p_(reinterpret_cast<char*>(this + 1))
    {
    }

public:
    static
    static_pool&
    construct(std::size_t size)
    {
        auto p = new char[sizeof(static_pool) + size];
        return *(::new(p) static_pool{size});
    }

    static_pool&
    share()
    {
        ++refs_;
        return *this;
    }

    void
    destroy()
    {
        if(refs_--)
            return;
        this->~static_pool();
        delete[] reinterpret_cast<char*>(this);
    }

    void*
    alloc(std::size_t n)
    {
        auto last = p_ + n;
        if(last >= end())
            BOOST_THROW_EXCEPTION(std::bad_alloc{});
        ++count_;
        auto p = p_;
        p_ = last;
        return p;
    }

    void
    dealloc()
    {
        if(--count_)
            return;
        p_ = reinterpret_cast<char*>(this + 1);
    }
};

} // detail

/** A non-thread-safe allocator optimized for @ref basic_fields.

    This allocator obtains memory from a pre-allocated memory block
    of a given size. It does nothing in deallocate until all
    previously allocated blocks are deallocated, upon which it
    resets the internal memory block for re-use.

    To use this allocator declare an instance persistent to the
    connection or session, and construct with the block size.
    A good rule of thumb is 20% more than the maximum allowed
    header size. For example if the application only allows up
    to an 8,000 byte header, the block size could be 9,600.

    Then, for every instance of `message` construct the header
    with a copy of the previously declared allocator instance.
*/
template<class T>
struct fields_alloc
{
    detail::static_pool* pool_;

public:
    using value_type = T;
    using is_always_equal = std::false_type;
    using pointer = T*;
    using reference = T&;
    using const_pointer = T const*;
    using const_reference = T const&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<class U>
    struct rebind
    {
        using other = fields_alloc<U>;
    };

#if defined(_GLIBCXX_USE_CXX11_ABI) && (_GLIBCXX_USE_CXX11_ABI == 0)
    // Workaround for g++
    // basic_string assumes that allocators are default-constructible
    // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
    fields_alloc() = default;
#endif

    explicit
    fields_alloc(std::size_t size)
        : pool_(&detail::static_pool::construct(size))
    {
    }

    fields_alloc(fields_alloc const& other)
        : pool_(&other.pool_->share())
    {
    }

    template<class U>
    fields_alloc(fields_alloc<U> const& other)
        : pool_(&other.pool_->share())
    {
    }

    ~fields_alloc()
    {
        pool_->destroy();
    }

    value_type*
    allocate(size_type n)
    {
        return static_cast<value_type*>(
            pool_->alloc(n * sizeof(T)));
    }

    void
    deallocate(value_type*, size_type)
    {
        pool_->dealloc();
    }

#if defined(BOOST_LIBSTDCXX_VERSION) && BOOST_LIBSTDCXX_VERSION < 60000
    template<class U, class... Args>
    void
    construct(U* ptr, Args&&... args)
    {
        ::new(static_cast<void*>(ptr)) U(
            std::forward<Args>(args)...);
    }

    template<class U>
    void
    destroy(U* ptr)
    {
        ptr->~U();
    }
#endif

    template<class U>
    friend
    bool
    operator==(
        fields_alloc const& lhs,
        fields_alloc<U> const& rhs)
    {
        return &lhs.pool_ == &rhs.pool_;
    }

    template<class U>
    friend
    bool
    operator!=(
        fields_alloc const& lhs,
        fields_alloc<U> const& rhs)
    {
        return ! (lhs == rhs);
    }
};

#endif
