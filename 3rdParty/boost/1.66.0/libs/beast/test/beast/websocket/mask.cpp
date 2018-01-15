//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/detail/mask.hpp>

#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class mask_test : public beast::unit_test::suite
{
public:
    struct test_generator
    {
        using result_type = std::uint32_t;

        result_type n = 0;

        void
        seed(std::seed_seq const&)
        {
        }

        void
        seed(result_type const&)
        {
        }

        std::uint32_t
        operator()()
        {
            return n++;
        }
    };

    void run() override
    {
        maskgen_t<test_generator> mg;
        BEAST_EXPECT(mg() != 0);
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,mask);

} // detail
} // websocket
} // beast
} // boost
