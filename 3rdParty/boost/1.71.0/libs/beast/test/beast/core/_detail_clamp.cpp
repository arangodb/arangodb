//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/clamp.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <climits>

namespace boost {
namespace beast {
namespace detail {

class clamp_test : public beast::unit_test::suite
{
public:
    void testClamp()
    {
        BEAST_EXPECT(clamp(
            (std::numeric_limits<std::uint64_t>::max)()) ==
                (std::numeric_limits<std::size_t>::max)());
    }

    void run() override
    {
        testClamp();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,clamp);

} // detail
} // beast
} // boost
