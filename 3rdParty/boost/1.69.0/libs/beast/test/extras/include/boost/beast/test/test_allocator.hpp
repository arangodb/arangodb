//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_TEST_ALLOCATOR_HPP
#define BOOST_BEAST_TEST_TEST_ALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <memory>

namespace boost {
namespace beast {
namespace test {

struct test_allocator_info
{
    std::size_t id;
    std::size_t ncopy = 0;
    std::size_t nmove = 0;
    std::size_t nmassign = 0;
    std::size_t ncpassign = 0;
    std::size_t nselect = 0;

    test_allocator_info()
        : id([]
            {
                static std::atomic<std::size_t> sid(0);
                return ++sid;
            }())
    {
    }
};

template<class T, bool Equal, bool Assign, bool Move, bool Swap, bool Select>
class test_allocator;

template<class T, bool Equal, bool Assign, bool Move, bool Swap, bool Select>
struct test_allocator_base
{
};

template<class T, bool Equal, bool Assign, bool Move, bool Swap>
struct test_allocator_base<T, Equal, Assign, Move, Swap, true>
{
    static
    test_allocator<T, Equal, Assign, Move, Swap, true>
    select_on_container_copy_construction(test_allocator<
        T, Equal, Assign, Move, Swap, true> const& a)
    {
        return test_allocator<T, Equal, Assign, Move, Swap, true>{};
    }
};

template<class T, bool Equal, bool Assign, bool Move, bool Swap, bool Select>
class test_allocator : public test_allocator_base<
        T, Equal, Assign, Move, Swap, Select>
{
    std::shared_ptr<test_allocator_info> info_;

    template<class, bool, bool, bool, bool, bool>
    friend class test_allocator;

public:
    using value_type = T;

    using propagate_on_container_copy_assignment =
        std::integral_constant<bool, Assign>;

    using propagate_on_container_move_assignment =
        std::integral_constant<bool, Move>;

    using propagate_on_container_swap =
        std::integral_constant<bool, Swap>;

    template<class U>
    struct rebind
    {
        using other = test_allocator<U, Equal, Assign, Move, Swap, Select>;
    };

    test_allocator()
        : info_(std::make_shared<test_allocator_info>())
    {
    }

    test_allocator(test_allocator const& u) noexcept
        : info_(u.info_)
    {
        ++info_->ncopy;
    }

    template<class U>
    test_allocator(test_allocator<U,
            Equal, Assign, Move, Swap, Select> const& u) noexcept
        : info_(u.info_)
    {
        ++info_->ncopy;
    }

    test_allocator(test_allocator&& t)
        : info_(t.info_)
    {
        ++info_->nmove;
    }

    test_allocator&
    operator=(test_allocator const& u) noexcept
    {
        info_ = u.info_;
        ++info_->ncpassign;
        return *this;
    }

    test_allocator&
    operator=(test_allocator&& u) noexcept
    {
        info_ = u.info_;
        ++info_->nmassign;
        return *this;
    }

    value_type*
    allocate(std::size_t n)
    {
        return static_cast<value_type*>(
            ::operator new (n*sizeof(value_type)));
    }

    void
    deallocate(value_type* p, std::size_t) noexcept
    {
        ::operator delete(p);
    }

    bool
    operator==(test_allocator const& other) const
    {
        return id() == other.id() || Equal;
    }

    bool
    operator!=(test_allocator const& other) const
    {
        return ! this->operator==(other);
    }

    std::size_t
    id() const
    {
        return info_->id;
    }

    test_allocator_info const*
    operator->() const
    {
        return info_.get();
    }
};

} // test
} // beast
} // boost

#endif
