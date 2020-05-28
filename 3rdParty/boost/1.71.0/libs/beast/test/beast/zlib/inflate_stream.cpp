//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/zlib/inflate_stream.hpp>

#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <chrono>
#include <random>

#include "zlib-1.2.11/zlib.h"

namespace boost {
namespace beast {
namespace zlib {

class inflate_stream_test : public beast::unit_test::suite
{
public:
    // Lots of repeats, limited char range
    static
    std::string
    corpus1(std::size_t n)
    {
        static std::string const alphabet{
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        };
        std::string s;
        s.reserve(n + 5);
        std::mt19937 g;
        std::uniform_int_distribution<std::size_t> d0{
            0, alphabet.size() - 1};
        std::uniform_int_distribution<std::size_t> d1{
            1, 5};
        while(s.size() < n)
        {
            auto const rep = d1(g);
            auto const ch = alphabet[d0(g)];
            s.insert(s.end(), rep, ch);
        }
        s.resize(n);
        return s;
    }

    // Random data
    static
    std::string
    corpus2(std::size_t n)
    {
        std::string s;
        s.reserve(n);
        std::mt19937 g;
        std::uniform_int_distribution<std::uint32_t> d0{0, 255};
        while(n--)
            s.push_back(static_cast<char>(d0(g)));
        return s;
    }

    static
    std::string
    compress(
        string_view const& in,
        int level,                  // 0=none, 1..9, -1=default
        int windowBits,             // 9..15
        int memLevel,               // 1..9 (8=default)
        int strategy)               // e.g. Z_DEFAULT_STRATEGY
    {
        int result;
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        result = deflateInit2(
            &zs,
            level,
            Z_DEFLATED,
            -windowBits,
            memLevel,
            strategy);
        if(result != Z_OK)
            throw std::logic_error{"deflateInit2 failed"};
        zs.next_in = (Bytef*)in.data();
        zs.avail_in = static_cast<uInt>(in.size());
        std::string out;
        out.resize(deflateBound(&zs,
            static_cast<uLong>(in.size())));
        zs.next_in = (Bytef*)in.data();
        zs.avail_in = static_cast<uInt>(in.size());
        zs.next_out = (Bytef*)&out[0];
        zs.avail_out = static_cast<uInt>(out.size());
        result = deflate(&zs, Z_FULL_FLUSH);
        if(result != Z_OK)
            throw std::logic_error("deflate failed");
        out.resize(zs.total_out);
        deflateEnd(&zs);
        return out;
    }

    //--------------------------------------------------------------------------

    enum Split
    {
        once,
        half,
        full
    };

    class Beast
    {
        Split in_;
        Split check_;
        Flush flush_;

    public:
        Beast(Split in, Split check, Flush flush = Flush::sync)
            : in_(in)
            , check_(check)
            , flush_(flush)
        {
        }

        void
        operator()(
            int window,
            std::string const& in,
            std::string const& check,
            unit_test::suite& suite) const
        {
            auto const f =
            [&](std::size_t i, std::size_t j)
            {
                std::string out(check.size(), 0);
                z_params zs;
                zs.next_in = in.data();
                zs.next_out = &out[0];
                zs.avail_in = i;
                zs.avail_out = j;
                inflate_stream is;
                is.reset(window);
                bool bi = ! (i < in.size());
                bool bo = ! (j < check.size());
                for(;;)
                {
                    error_code ec;
                    is.write(zs, flush_, ec);
                    if( ec == error::need_buffers ||
                        ec == error::end_of_stream)
                    {
                        out.resize(zs.total_out);
                        suite.expect(out == check, __FILE__, __LINE__);
                        break;
                    }
                    if(ec)
                    {
                        suite.fail(ec.message(), __FILE__, __LINE__);
                        break;
                    }
                    if(zs.avail_in == 0 && ! bi)
                    {
                        bi = true;
                        zs.avail_in = in.size() - i;
                    }
                    if(zs.avail_out == 0 && ! bo)
                    {
                        bo = true;
                        zs.avail_out = check.size() - j;
                    }
                }
            };

            std::size_t i0, i1;
            std::size_t j0, j1;

            switch(in_)
            {
            default:
            case once: i0 = in.size();     i1 = i0;         break;
            case half: i0 = in.size() / 2; i1 = i0;         break;
            case full: i0 = 1;             i1 = in.size();  break;
            }

            switch(check_)
            {
            default:
            case once: j0 = check.size();     j1 = j0;              break;
            case half: j0 = check.size() / 2; j1 = j0;              break;
            case full: j0 = 1;                j1 = check.size();    break;
            }

            for(std::size_t i = i0; i <= i1; ++i)
                for(std::size_t j = j0; j <= j1; ++j)
                    f(i, j);
        }
    };

    class Matrix
    {
        unit_test::suite& suite_;

        int level_[2];
        int window_[2];
        int strategy_[2];

    public:
        explicit
        Matrix(unit_test::suite& suite)
            : suite_(suite)
        {
            level_[0] = 0;
            level_[1] = 9;
            window_[0] = 9;
            window_[1] = 15;
            strategy_[0] = 0;
            strategy_[1] = 4;
        }

        void
        level(int from, int to)
        {
            level_[0] = from;
            level_[1] = to;
        }

        void
        level(int what)
        {
            level(what, what);
        }

        void
        window(int from, int to)
        {
            window_[0] = from;
            window_[1] = to;
        }

        void
        window(int what)
        {
            window(what, what);
        }

        void
        strategy(int from, int to)
        {
            strategy_[0] = from;
            strategy_[1] = to;
        }

        void
        strategy(int what)
        {
            strategy(what, what);
        }

        template<class F>
        void
        operator()(
            F const& f,
            std::string const& check) const
        {
            for(auto level = level_[0];
                level <= level_[1]; ++level)
            {
                for(auto window = window_[0];
                    window <= window_[1]; ++window)
                {
                    for(auto strategy = strategy_[0];
                        strategy <= strategy_[1]; ++strategy)
                        f(
                            window,
                            compress(check, level, window, 4, strategy),
                            check,
                            suite_);
                }
            }
        }
    };

    void
    testInflate()
    {
        {
            Matrix m{*this};
            std::string check =
                "{\n   \"AutobahnPython/0.6.0\": {\n"
                "      \"1.1.1\": {\n"
                "         \"behavior\": \"OK\",\n"
                "         \"behaviorClose\": \"OK\",\n"
                "         \"duration\": 2,\n"
                "         \"remoteCloseCode\": 1000,\n"
                "         \"reportfile\": \"autobahnpython_0_6_0_case_1_1_1.json\"\n"
                ;
            m(Beast{half, half}, check);
        }

        {
            Matrix m{*this};
            auto const check = corpus1(5000);
            m(Beast{half, half}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus2(5000);
            m(Beast{half, half}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus1(1000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{once, full}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus2(1000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{once, full}, check);
        }
        {
            Matrix m{*this};
            m.level(6);
            m.window(9);
            auto const check = corpus1(200);
            m(Beast{full, full}, check);
        }
        {
            Matrix m{*this};
            m.level(6);
            m.window(9);
            auto const check = corpus2(500);
            m(Beast{full, full}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus2(1000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{full, once, Flush::block}, check);
        }

        // VFALCO Fails, but I'm unsure of what the correct
        //        behavior of Z_TREES/Flush::trees is.
#if 0
        {
            Matrix m{*this};
            auto const check = corpus2(10000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{full, once, Flush::trees}, check);
        }
#endif
    }

    void
    run() override
    {
        log <<
            "sizeof(inflate_stream) == " <<
            sizeof(inflate_stream) << std::endl;
        testInflate();
    }
};

BEAST_DEFINE_TESTSUITE(beast,zlib,inflate_stream);

} // zlib
} // beast
} // boost
