//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/chunk_encode.hpp>

#include "message_fuzz.hpp"

#include <boost/beast/core/static_string.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/test/fuzz.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/optional.hpp>
#include <random>

namespace boost {
namespace beast {
namespace http {

class chunk_encode_test
    : public beast::unit_test::suite
{
public:
    struct not_chunk_extensions {};

    BOOST_STATIC_ASSERT(
        detail::is_chunk_extensions<chunk_extensions>::value);
    
    BOOST_STATIC_ASSERT(
        ! detail::is_chunk_extensions<not_chunk_extensions>::value);

    template<class ConstBufferSequence>
    static
    std::string
    to_string(ConstBufferSequence const& buffers)
    {
        std::string s;
        s.reserve(boost::asio::buffer_size(buffers));
        for(boost::asio::const_buffer b : beast::detail::buffers_range(buffers))
            s.append(
                reinterpret_cast<char const*>(b.data()),
                b.size());
        return s;
    }

    template<class T, class... Args>
    void
    check(string_view match, Args&&... args)
    {
        T t(std::forward<Args>(args)...);
        BEAST_EXPECT(to_string(t) == match);
        T t2(t);
        BEAST_EXPECT(to_string(t2) == match);
        T t3(std::move(t2));
        BEAST_EXPECT(to_string(t3) == match);
    }

    template<class T, class... Args>
    void
    check_fwd(string_view match, Args&&... args)
    {
        T t(std::forward<Args>(args)...);
        BEAST_EXPECT(to_string(t) == match);
        T t2(t);
        BEAST_EXPECT(to_string(t2) == match);
        T t3(std::move(t2));
        BEAST_EXPECT(to_string(t3) == match);
    }

    using cb_t = boost::asio::const_buffer;

    static
    cb_t
    cb(string_view s)
    {
        return {s.data(), s.size()};
    }

    void
    testChunkCRLF()
    {
    #if ! defined(BOOST_GCC) || BOOST_GCC >= 50000
        check<chunk_crlf>("\r\n");
    #endif
    }

    void
    testChunkHeader()
    {
        check<chunk_header>("10\r\n", 16u);

        check<chunk_header>("20;x\r\n", 32u, ";x");

        chunk_extensions exts;
        exts.insert("y");
        exts.insert("z");

        check<chunk_header>("30;y;z\r\n", 48u, exts);

        {
            auto exts2 = exts;

            check_fwd<chunk_header>(
                "30;y;z\r\n", 48u, std::move(exts2));
        }

        check<chunk_header>("30;y;z\r\n", 48u, exts,
            std::allocator<double>{});

        {
            auto exts2 = exts;

            check<chunk_header>(
                "30;y;z\r\n", 48u, std::move(exts2),
                std::allocator<double>{});
        }
    }

    void
    testChunkBody()
    {
        check<chunk_body<cb_t>>(
            "3\r\n***\r\n", cb("***"));
        
        check<chunk_body<cb_t>>(
            "3;x\r\n***\r\n", cb("***"), ";x");

        chunk_extensions exts;
        exts.insert("y");
        exts.insert("z");

        check<chunk_body<cb_t>>(
            "3;y;z\r\n***\r\n",
            cb("***"), exts);

        {
            auto exts2 = exts;

            check_fwd<chunk_body<cb_t>>(
                "3;y;z\r\n***\r\n",
                cb("***"), std::move(exts2));
        }

        check<chunk_body<cb_t>>(
            "3;y;z\r\n***\r\n",
            cb("***"), exts, std::allocator<double>{});

        {
            auto exts2 = exts;

            check_fwd<chunk_body<cb_t>>(
                "3;y;z\r\n***\r\n",
                cb("***"), std::move(exts2),
                std::allocator<double>{});
        }
    }

    void
    testChunkFinal()
    {
        check<chunk_last<>>(
            "0\r\n\r\n");

        check<chunk_last<cb_t>>(
            "0\r\nMD5:ou812\r\n\r\n",
            cb("MD5:ou812\r\n\r\n"));

        fields trailers;
        trailers.set(field::content_md5, "ou812");

        check<chunk_last<fields>>(
            "0\r\nContent-MD5: ou812\r\n\r\n",
            trailers);

        {
            auto trailers2 = trailers;

            check_fwd<chunk_last<fields>>(
                "0\r\nContent-MD5: ou812\r\n\r\n",
                std::move(trailers2));
        }

        check<chunk_last<fields>>(
            "0\r\nContent-MD5: ou812\r\n\r\n",
            trailers, std::allocator<double>{});

        {
            auto trailers2 = trailers;

            check<chunk_last<fields>>(
                "0\r\nContent-MD5: ou812\r\n\r\n",
                std::move(trailers2), std::allocator<double>{});
        }
    }

    void
    testChunkExtensions()
    {
        auto const str =
            [](chunk_extensions const& ce)
            {
                std::string s;
                for(auto const& v : ce)
                {
                    s.append(v.first.to_string());
                    s.push_back(',');
                    if(! v.second.empty())
                    {
                        s.append(v.second.to_string());
                        s.push_back(',');
                    }
                }
                return s;
            };
        chunk_extensions ce;
        ce.insert("x");
        BEAST_EXPECT(ce.str() == ";x");
        BEAST_EXPECT(str(ce) == "x,");
        ce.insert("y", "z");
        BEAST_EXPECT(ce.str() == ";x;y=z");
        BEAST_EXPECT(str(ce) == "x,y,z,");
        ce.insert("z", R"(")");
        BEAST_EXPECT(ce.str() == R"(;x;y=z;z="\"")");
        BEAST_EXPECT(str(ce)  == R"(x,y,z,z,",)");
        ce.insert("p", R"(\)");
        BEAST_EXPECT(ce.str() == R"(;x;y=z;z="\"";p="\\")");
        BEAST_EXPECT(str(ce)  == R"(x,y,z,z,",p,\,)");
        ce.insert("q", R"(1"2\)");
        BEAST_EXPECT(ce.str() == R"(;x;y=z;z="\"";p="\\";q="1\"2\\")");
        BEAST_EXPECT(str(ce)  == R"(x,y,z,z,",p,\,q,1"2\,)");
    }

    void
    testParseChunkExtensions()
    {
        auto const grind =
        [&](string_view s)
        {
            error_code ec;
            static_string<200> ss{s};
            test::fuzz_rand r;
            for(auto i = 3; i--;)
            {
                test::fuzz(ss, 5, 5, r,
                [&](string_view s)
                {
                    chunk_extensions c1;
                    c1.parse(s, ec);
                    if(ec)
                    {
                        pass();
                        return;
                    }
                    chunk_extensions c2;
                    c2.parse(c1.str(), ec);
                    if(! BEAST_EXPECTS(! ec, ec.message()))
                        return;
                    chunk_extensions c3;
                    for(auto const& v : c2)
                        if(v.second.empty())
                            c3.insert(v.first);
                        else
                            c3.insert(v.first, v.second);
                    BEAST_EXPECTS(c2.str() == c3.str(), c3.str());
                });
            }
        };
        auto const good =
        [&](string_view s)
        {
            error_code ec;
            chunk_extensions ce;
            ce.parse(s, ec);
            BEAST_EXPECTS(! ec, ec.message());
            grind(s);
        };
        auto const bad =
        [&](string_view s)
        {
            error_code ec;
            chunk_extensions ce;
            ce.parse(s, ec);
            BEAST_EXPECT(ec);
            grind(s);
        };
        chunkExtensionsTest(good, bad);
    }

    void
    run() override
    {
        testChunkCRLF();
        testChunkHeader();
        testChunkBody();
        testChunkFinal();
        testChunkExtensions();
        testParseChunkExtensions();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,chunk_encode);

} // http
} // beast
} // boost
