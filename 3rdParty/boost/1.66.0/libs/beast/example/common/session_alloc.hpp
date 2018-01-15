//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_COMMON_SESSION_ALLOC_HPP
#define BOOST_BEAST_EXAMPLE_COMMON_SESSION_ALLOC_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/intrusive/list.hpp>
#include <algorithm>
#include <cstddef>
#include <utility>

namespace detail {

template<class Context>
class session_alloc_base
{
protected:
    class pool_t
    {
        using hook_type = 
            boost::intrusive::list_base_hook<
                boost::intrusive::link_mode<
                    boost::intrusive::normal_link>>;

        class element : public hook_type
        {
            std::size_t size_;
            std::size_t used_;
            // add padding here
        
        public:
            explicit
            element(std::size_t size, std::size_t used)
                : size_(size)
                , used_(used)
            {
            }

            std::size_t
            size() const
            {
                return size_;
            }

            std::size_t
            used() const
            {
                return used_;
            }

            char*
            end() const
            {
                return data() + size_;
            }

            char*
            data() const
            {
                return const_cast<char*>(
                    reinterpret_cast<
                        char const *>(this + 1));
            }
        };

        using list_type = typename
            boost::intrusive::make_list<element,
                boost::intrusive::constant_time_size<
                    true>>::type;

        std::size_t refs_ = 1;      // shared count
        std::size_t high_ = 0;      // highest used
        std::size_t size_ = 0;      // size of buf_
        char* buf_ = nullptr;       // a large block
        list_type list_;            // list of allocations

        pool_t() = default;

    public:
        static
        pool_t&
        construct();

        ~pool_t();

        pool_t&
        addref();

        void
        release();

        void*
        alloc(std::size_t n);

        void
        dealloc(void* pv, std::size_t n);
    };
};

template<class Context>
auto
session_alloc_base<Context>::
pool_t::
construct() ->
    pool_t&
{
    return *(new pool_t);
}

template<class Context>
session_alloc_base<Context>::
pool_t::
~pool_t()
{
    BOOST_ASSERT(list_.size() == 0);
    if(buf_)
        delete[] buf_;
}

template<class Context>
auto
session_alloc_base<Context>::
pool_t::
addref() ->
    pool_t&
{
    ++refs_;
    return *this;
}

template<class Context>
void
session_alloc_base<Context>::
pool_t::
release()
{
    if(--refs_)
        return;
    delete this;
}

template<class Context>
void*
session_alloc_base<Context>::
pool_t::
alloc(std::size_t n)
{
    if(list_.empty() && size_ < high_)
    {
        if(buf_)
            delete[] buf_;
        buf_ = new char[high_];
        size_ = high_;
    }
    if(buf_)
    {
        char* end;
        std::size_t used;
        if(list_.empty())
        {
            end = buf_;
            used = sizeof(element) + n;
        }
        else
        {
            end = list_.back().end();
            used = list_.back().used() +
                sizeof(element) + n;
        }
        if(end >= buf_ && end +
            sizeof(element) + n <= buf_ + size_)
        {
            auto& e = *new(end) element{n, used};
            list_.push_back(e);
            high_ = (std::max)(high_, used);
            return e.data();
        }
    }
    std::size_t const used =
        sizeof(element) + n + (
            buf_ && ! list_.empty() ?
                list_.back().used() : 0);
    auto& e = *new(new char[sizeof(element) + n]) element{n, used};
    list_.push_back(e);
    high_ = (std::max)(high_, used);
    return e.data();
}

template<class Context>
void
session_alloc_base<Context>::
pool_t::
dealloc(void* pv, std::size_t n)
{
    boost::ignore_unused(n);
    auto& e = *(reinterpret_cast<element*>(pv) - 1);
    BOOST_ASSERT(e.size() == n);
    if( (e.end() > buf_ + size_) ||
        reinterpret_cast<char*>(&e) < buf_)
    {
        list_.erase(list_.iterator_to(e));
        e.~element();
        delete[] reinterpret_cast<char*>(&e);
        return;
    }
    list_.erase(list_.iterator_to(e));
    e.~element();
}

} // detail

//------------------------------------------------------------------------------

namespace detail {
template<class Handler>
class session_alloc_wrapper;
} // detail

template<class T>
class session_alloc
    : private detail::session_alloc_base<void>
{
    template<class U>
    friend class session_alloc;

    using pool_t = typename
        detail::session_alloc_base<void>::pool_t;

    pool_t& pool_;

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
        using other = session_alloc<U>;
    };

    session_alloc& operator=(session_alloc const&) = delete;

    ~session_alloc()
    {
        pool_.release();
    }

    session_alloc()
        : pool_(pool_t::construct())
    {
    }

    session_alloc(session_alloc const& other) noexcept
        : pool_(other.pool_.addref())
    {
    }

    template<class U>
    session_alloc(session_alloc<U> const& other) noexcept
        : pool_(other.pool_)
    {
    }

    template<class Handler>
    detail::session_alloc_wrapper<typename std::decay<Handler>::type>
    wrap(Handler&& handler);

    value_type*
    allocate(size_type n)
    {
        return static_cast<value_type*>(
            this->alloc(n * sizeof(T)));
    }

    void
    deallocate(value_type* p, size_type n)
    {
        this->dealloc(p, n * sizeof(T));
    }

#if defined(BOOST_LIBSTDCXX_VERSION) && BOOST_LIBSTDCXX_VERSION < 60000
    template<class U, class... Args>
    void
    construct(U* ptr, Args&&... args)
    {
        ::new((void*)ptr) U(std::forward<Args>(args)...);
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
        session_alloc const& lhs,
        session_alloc<U> const& rhs)
    {
        return &lhs.pool_ == &rhs.pool_;
    }

    template<class U>
    friend
    bool
    operator!=(
        session_alloc const& lhs,
        session_alloc<U> const& rhs)
    {
        return ! (lhs == rhs);
    }

protected:
    void*
    alloc(std::size_t n)
    {
        return pool_.alloc(n);
    }

    void
    dealloc(void* p, std::size_t n)
    {
        pool_.dealloc(p, n);
    }
};

//------------------------------------------------------------------------------

namespace detail {

template<class Handler>
class session_alloc_wrapper
{
    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class U, class Executor>
    friend struct boost::asio::associated_executor;

    Handler h_;
    session_alloc<char> alloc_;

public:
    session_alloc_wrapper(session_alloc_wrapper&&) = default;
    session_alloc_wrapper(session_alloc_wrapper const&) = default;

    template<class DeducedHandler>
    session_alloc_wrapper(
        DeducedHandler&& h,
        session_alloc<char> const& alloc)
        : h_(std::forward<DeducedHandler>(h))
        , alloc_(alloc)
    {
    }

    using allocator_type = session_alloc<char>;

    allocator_type
    get_allocator() const noexcept;

    template<class... Args>
    void
    operator()(Args&&... args) const
    {
        h_(std::forward<Args>(args)...);
    }

    friend
    bool
    asio_handler_is_continuation(session_alloc_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }
};

template<class Handler>
auto
session_alloc_wrapper<Handler>::
get_allocator() const noexcept ->
    allocator_type
{
    return alloc_;
}

} // detail

//------------------------------------------------------------------------------

template<class T>
template<class Handler>
auto
session_alloc<T>::
wrap(Handler&& handler) ->
    detail::session_alloc_wrapper<typename std::decay<Handler>::type>
{
    return detail::session_alloc_wrapper<
        typename std::decay<Handler>::type>(
            std::forward<Handler>(handler), *this);
}

//------------------------------------------------------------------------------

namespace boost {
namespace asio {
template<class Handler, class Executor>
struct associated_executor<
    ::detail::session_alloc_wrapper<Handler>, Executor>
{
    using type = typename
        associated_executor<Handler, Executor>::type;

    static
    type
    get(::detail::session_alloc_wrapper<Handler> const& h,
        Executor const& ex = Executor()) noexcept
    {
        return associated_executor<
            Handler, Executor>::get(h.h_, ex);
    }
};

} // asio
} // boost

#endif
