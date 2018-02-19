//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_THROUGHPUT_HPP
#define BOOST_BEAST_TEST_THROUGHPUT_HPP

#include <chrono>
#include <cstdint>

namespace boost {
namespace beast {
namespace test {

class timer
{
    using clock_type =
        std::chrono::system_clock;

    clock_type::time_point when_;

public:
    using duration =
        clock_type::duration;

    timer()
        : when_(clock_type::now())
    {
    }

    duration
    elapsed() const
    {
        return clock_type::now() - when_;
    }
};

inline
std::uint64_t
throughput(std::chrono::duration<
    double> const& elapsed, std::uint64_t items)
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        1 / (elapsed/items).count());
}

} // test
} // beast
} // boost

#endif
