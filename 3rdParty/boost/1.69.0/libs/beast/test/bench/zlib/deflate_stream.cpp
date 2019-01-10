//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/string.hpp>
#include <boost/beast/zlib/deflate_stream.hpp>
#include <boost/beast/test/throughput.hpp>
#include <boost/beast/unit_test/dstream.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <iomanip>
#include <random>
#include <string>

#include "zlib-1.2.11/zlib.h"

namespace boost {
namespace beast {
namespace zlib {

class deflate_stream_test : public beast::unit_test::suite
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

    std::string
    doDeflateBeast(string_view const& in)
    {
        z_params zs;
        memset(&zs, 0, sizeof(zs));
        deflate_stream ds;
        ds.reset(
            Z_DEFAULT_COMPRESSION,
            15,
            4,
            Strategy::normal);
        std::string out;
        out.resize(deflate_upper_bound(in.size()));
        zs.next_in = in.data();
        zs.avail_in = in.size();
        zs.next_out = &out[0];
        zs.avail_out = out.size();
        error_code ec;
        ds.write(zs, Flush::full, ec);
        BEAST_EXPECTS(! ec, ec.message());
        out.resize(zs.total_out);
        return out;
    }

    std::string
    doDeflateZLib(string_view const& in)
    {
        int result;
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        result = deflateInit2(
            &zs,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -15,
            4,
            Z_DEFAULT_STRATEGY
        );
        if(result != Z_OK)
            throw std::logic_error("deflateInit2 failed");
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

    void
    doCorpus(
        std::size_t size,
        std::size_t repeat)
    {
        std::size_t constexpr trials = 3;
        auto const c1 = corpus1(size);
        auto const c2 = corpus2(size);
        log <<
            std::left << std::setw(10) << (std::to_string(size) + "B") <<
            std::right << std::setw(12) << "Beast" << "     " <<
            std::right << std::setw(12) << "ZLib" <<
                std::endl;
        for(std::size_t i = 0; i < trials; ++i)
        {
            test::timer t;
            log << std::left << std::setw(10) << "corpus1";
            std::string out1;
            for(std::size_t j = 0; j < repeat; ++j)
                out1 = doDeflateBeast(c1);
            auto const t1 =
                test::throughput(t.elapsed(), size * repeat);
            log << std::right << std::setw(12) << t1 << " B/s ";
            std::string out2;
            for(std::size_t j = 0; j < repeat; ++j)
                out2 = doDeflateZLib(c1);
            BEAST_EXPECT(out1 == out2);
            auto const t2 =
                test::throughput(t.elapsed(), size * repeat);
            log << std::right << std::setw(12) << t2 << " B/s";
            log << std::right << std::setw(12) <<
                unsigned(double(t1)*100/t2-100) << "%";
            log << std::endl;
        }
        for(std::size_t i = 0; i < trials; ++i)
        {
            test::timer t;
            log << std::left << std::setw(10) << "corpus2";
            std::string out1;
            for(std::size_t j = 0; j < repeat; ++j)
                out1 = doDeflateBeast(c2);
            auto const t1 =
                test::throughput(t.elapsed(), size * repeat);
            log << std::right << std::setw(12) << t1 << " B/s ";
            std::string out2;
            for(std::size_t j = 0; j < repeat; ++j)
                out2 = doDeflateZLib(c2);
            BEAST_EXPECT(out1 == out2);
            auto const t2 =
                test::throughput(t.elapsed(), size * repeat);
            log << std::right << std::setw(12) << t2 << " B/s";
            log << std::right << std::setw(12) <<
                unsigned(double(t1)*100/t2-100) << "%";
            log << std::endl;
        }
        log << std::endl;
    }

    void
    doBench()
    {
        doCorpus(      16 * 1024, 512);
        doCorpus(    1024 * 1024,   8);
        doCorpus(8 * 1024 * 1024,   1);
    }

    void
    run() override
    {
        doBench();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,zlib,deflate_stream);

} // zlib
} // beast
} // boost
