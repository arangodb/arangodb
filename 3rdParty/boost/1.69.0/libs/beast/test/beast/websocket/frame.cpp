//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class frame_test
    : public beast::unit_test::suite
{
public:
    void testCloseCodes()
    {
        BEAST_EXPECT(! is_valid_close_code(0));
        BEAST_EXPECT(! is_valid_close_code(1));
        BEAST_EXPECT(! is_valid_close_code(999));
        BEAST_EXPECT(! is_valid_close_code(1004));
        BEAST_EXPECT(! is_valid_close_code(1005));
        BEAST_EXPECT(! is_valid_close_code(1006));
        BEAST_EXPECT(! is_valid_close_code(1016));
        BEAST_EXPECT(! is_valid_close_code(2000));
        BEAST_EXPECT(! is_valid_close_code(2999));
        BEAST_EXPECT(is_valid_close_code(1000));
        BEAST_EXPECT(is_valid_close_code(1002));
        BEAST_EXPECT(is_valid_close_code(3000));
        BEAST_EXPECT(is_valid_close_code(4000));
        BEAST_EXPECT(is_valid_close_code(5000));
    }

    struct test_fh : frame_header
    {
        test_fh()
        {
            op = detail::opcode::text;
            fin =  true;
            mask = false;
            rsv1 = false;
            rsv2 = false;
            rsv3 = false;
            len = 0;
            key = 0;
        }
    };

    void
    testWriteFrame()
    {
        test_fh fh;
        fh.rsv2 = true;
        fh.rsv3 = true;
        fh.len = 65536;
        frame_buffer fb;
        write(fb, fh);
    }

    void run() override
    {
        testWriteFrame();
        testCloseCodes();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,frame);

} // detail
} // websocket
} // beast
} // boost
