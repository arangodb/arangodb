//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/varint.hpp>

#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace detail {

class varint_test : public beast::unit_test::suite
{
public:
    void
    testVarint()
    {
        std::size_t n0 = 0;
        std::size_t n1 = 1;
        for(;;)
        {
            char buf[16];
            BOOST_ASSERT(sizeof(buf) >= varint_size(n0));
            auto it = &buf[0];
            varint_write(it, n0);
            it = &buf[0];
            auto n = varint_read(it);
            BEAST_EXPECT(n == n0);
            n = n0 + n1;
            if(n < n1)
                break;
            n0 = n1;
            n1 = n;
        }
    }

    void
    run()
    {
        testVarint();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,varint);

} // detail
} // beast
} // boost
