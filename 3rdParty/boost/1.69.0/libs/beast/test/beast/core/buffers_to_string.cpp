//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_to_string.hpp>

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {

class buffers_to_string_test : public unit_test::suite
{
public:
    void
    run() override
    {
        multi_buffer b;
        ostream(b) << "Hello, ";
        ostream(b) << "world!";
        BEAST_EXPECT(buffers_to_string(b.data()) == "Hello, world!");
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_to_string);

} // beast
} // boost
