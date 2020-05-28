//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/flat_stream.hpp>

#include "stream_tests.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/role.hpp>
#include <initializer_list>
#include <vector>

namespace boost {
namespace beast {

class flat_stream_test : public unit_test::suite
{
public:
    void
    testMembers()
    {
        net::io_context ioc;

        test_sync_stream<flat_stream<test::stream>>();

        test_async_stream<flat_stream<test::stream>>();

        // read/write

        {
            error_code ec;
            flat_stream<test::stream> s(ioc);
            {
                // VFALCO Hack to make test stream code = eof
                test::stream ts(ioc);
                s.next_layer().connect(ts);
            }
            char buf[1];
            net::mutable_buffer m1 = net::buffer(buf);

            BEAST_EXPECT(s.read_some(net::mutable_buffer{}) == 0);
            BEAST_EXPECT(s.read_some(net::mutable_buffer{}, ec) == 0);
            BEAST_EXPECTS(! ec, ec.message());

            try
            {
                s.read_some(m1);
                BEAST_FAIL();
            }
            catch(std::exception const&)
            {
                BEAST_PASS();
            }
            catch(...)
            {
                BEAST_FAIL();
            }

            BEAST_EXPECT(s.write_some(net::const_buffer{}) == 0);
            BEAST_EXPECT(s.write_some(net::const_buffer{}, ec) == 0);
            BEAST_EXPECTS(! ec, ec.message());

            try
            {
                s.write_some(m1);
                BEAST_FAIL();
            }
            catch(std::exception const&)
            {
                BEAST_PASS();
            }
            catch(...)
            {
                BEAST_FAIL();
            }

            bool invoked;

            invoked = false;
            s.async_read_some(net::mutable_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);

            invoked = false;
            s.async_write_some(net::const_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);
        }

        // stack_write_some

        {
            char b[detail::flat_stream_base::max_size];
            std::array<net::const_buffer, 3> bs;
            bs[0] = net::const_buffer(b, 100);
            bs[1] = net::const_buffer(b + 100, 200);
            bs[2] = net::const_buffer(b + 100 + 200, 300);
            BEAST_EXPECT(buffer_bytes(bs) <=
                detail::flat_stream_base::max_stack);
            flat_stream<test::stream> s(ioc);
            error_code ec;
            s.write_some(bs, ec);
        }

        // write_some

        {
            char b[detail::flat_stream_base::max_size];
            std::array<net::const_buffer, 2> bs;
            bs[0] = net::const_buffer(b,
                detail::flat_stream_base::max_stack);
            bs[1] = net::const_buffer(b + bs[0].size(), 1024);
            BEAST_EXPECT(buffer_bytes(bs) <=
                detail::flat_stream_base::max_size);
            flat_stream<test::stream> s(ioc);
            error_code ec;
            s.write_some(bs, ec);
        }

        // async_write_some

        {
            char b[detail::flat_stream_base::max_size];
            std::array<net::const_buffer, 2> bs;
            bs[0] = net::const_buffer(b,
                detail::flat_stream_base::max_stack);
            bs[1] = net::const_buffer(b + bs[0].size(), 1024);
            BEAST_EXPECT(buffer_bytes(bs) <=
                detail::flat_stream_base::max_size);
            flat_stream<test::stream> s(ioc);
            s.async_write_some(bs,
                [](error_code, std::size_t)
                {
                });
        }

        // teardown

        {
            test::stream ts(ioc);
            flat_stream<test::stream> s(ioc);
            ts.connect(s.next_layer());
            error_code ec;
            teardown(role_type::client, s, ec);
        }

        {
            test::stream ts(ioc);
            flat_stream<test::stream> s(ioc);
            ts.connect(s.next_layer());
            async_teardown(role_type::client, s,
                [](error_code)
                {
                });
        }
    }

    void
    testSplit()
    {
        auto const check =
            [&](
                std::initializer_list<int> v0,
                std::size_t limit,
                unsigned long count,
                bool copy)
            {
                std::vector<net::const_buffer> v;
                v.reserve(v0.size());
                for(auto const n : v0)
                    v.emplace_back("", n);
                auto const result =
                    boost::beast::detail::flat_stream_base::flatten(v, limit);
                BEAST_EXPECT(result.size == count);
                BEAST_EXPECT(result.flatten == copy);
                return result;
            };
        check({},           1,    0, false);
        check({1,2},        1,    1, false);
        check({1,2},        2,    1, false);
        check({1,2},        3,    3, true);
        check({1,2},        4,    3, true);
        check({1,2,3},      1,    1, false);
        check({1,2,3},      2,    1, false);
        check({1,2,3},      3,    3, true);
        check({1,2,3},      4,    3, true);
        check({1,2,3},      7,    6, true);
        check({1,2,3,4},    3,    3, true);
    }

    void
    run() override
    {
        testMembers();
        testSplit();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,flat_stream);

} // beast
} // boost
