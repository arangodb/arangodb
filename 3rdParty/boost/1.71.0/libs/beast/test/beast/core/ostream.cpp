//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/ostream.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <ostream>

namespace boost {
namespace beast {

class ostream_test : public beast::unit_test::suite
{
public:
    void
    testOstream()
    {
        string_view const s = "0123456789abcdef";
        BEAST_EXPECT(s.size() == 16);

        // overflow
        {
            flat_static_buffer<16> b;
            ostream(b) << s;
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        }

        // max_size
        {
            flat_static_buffer<16> b;
            auto os = ostream(b);
            os << s;
            os << '*';
            BEAST_EXPECT(os.bad());
        }

        // max_size (exception
        {
            flat_static_buffer<16> b;
            auto os = ostream(b);
            os.exceptions(os.badbit);
            os << s;
            try
            {
                os << '*';
                fail("missing exception", __FILE__, __LINE__);
            }
            catch(std::ios_base::failure const&)
            {
                pass();
            }
            catch(...)
            {
                fail("wrong exception", __FILE__, __LINE__);
            }
        }
    }

    void
    run() override
    {
        testOstream();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,ostream);

} // beast
} // boost
