//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/make_printable.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <iostream>
#include <sstream>

#include "test_buffer.hpp"

namespace boost {
namespace beast {

class make_printable_test : public beast::unit_test::suite
{
public:
    template <class ConstBufferSequence>
    void
    print (ConstBufferSequence const& buffers)
    {
        std::cout <<
            "Buffer size: " << buffer_bytes(buffers) << " bytes\n"
            "Buffer data: '" << make_printable(buffers) << "'\n";
    }

    void
    testJavadoc()
    {
        BEAST_EXPECT(&make_printable_test::print<buffers_triple>);
    }

    void
    testMakePrintable()
    {
        char buf[13];
        buffers_triple b(buf, sizeof(buf));
        string_view src = "Hello, world!";
        BEAST_EXPECT(src.size() == sizeof(buf));
        net::buffer_copy(b,
            net::const_buffer(src.data(), src.size()));
        std::ostringstream ss;
        ss << make_printable(b);
        BEAST_EXPECT(ss.str() == src);
    }

    void
    run() override
    {
        testJavadoc();
        testMakePrintable();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,make_printable);

} // beast
} // boost
