//
// Copyright (c) 2021 Dmitry Arkhipov (grisumbras@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_CHECKING_RESOURCE_HPP
#define BOOST_JSON_CHECKING_RESOURCE_HPP

#include <boost/json/storage_ptr.hpp>
#include <map>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class checking_resource
    : public memory_resource
{
    storage_ptr upstream_;
    std::map<void const*, std::pair<std::size_t, std::size_t>> allocs_;

public:
    explicit checking_resource(storage_ptr upstream = {}) noexcept
        : upstream_(std::move(upstream))
    {}

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override
    {
        void* result = upstream_->allocate(size, alignment);
        allocs_.emplace(result, std::make_pair(size, alignment));
        return result;
    }

    void do_deallocate(void* ptr, std::size_t size, std::size_t alignment)
        override
    {
        auto const it = allocs_.find(ptr);
        if( BOOST_TEST(it != allocs_.end()) )
        {
            BOOST_TEST(size == it->second.first);
            BOOST_TEST(alignment == it->second.second);

            allocs_.erase(it);
        }

        return upstream_->deallocate(ptr, size, alignment);
    }

    bool do_is_equal(memory_resource const& other) const noexcept
        override
    {
        return this == &other;
    }
};

BOOST_JSON_NS_END

#endif // BOOST_JSON_CHECKING_RESOURCE_HPP
