//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/detail/sha1.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <array>

namespace boost {
namespace beast {
namespace detail {

class sha1_test : public beast::unit_test::suite
{
public:
    static
    inline
    std::uint8_t
    unhex(char c)
    {
        if(c >= '0' && c <= '9')
            return c - '0';
        if(c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if(c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw std::invalid_argument("not a hex digit");
    }

    static
    std::string
    unhex(std::string const& in)
    {
        std::string out;
        out.reserve(in.size() / 2);
        if(in.size() % 2)
            throw std::domain_error("invalid hex string");
        for(std::size_t i = 0; i < in.size(); i += 2)
            out.push_back(
                (unhex(in[i])<<4) + unhex(in[i+1]));
        return out;
    }

    void
    check(std::string const& message, std::string const& answer)
    {
        std::string digest;
        digest = unhex(answer);
        sha1_context ctx;
        std::string result;
        result.resize(sha1_context::digest_size);
        init(ctx);
        update(ctx, message.data(), message.size());
        finish(ctx, &result[0]);
        BEAST_EXPECT(result == digest);
    }

    void
    run()
    {
        // http://www.di-mgt.com.au/sha_testvectors.html
        //
        check("abc",
            "a9993e36" "4706816a" "ba3e2571" "7850c26c" "9cd0d89d");
        check("",
            "da39a3ee" "5e6b4b0d" "3255bfef" "95601890" "afd80709");
        check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "84983e44" "1c3bd26e" "baae4aa1" "f95129e5" "e54670f1");
        check("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            "a49b2446" "a02c645b" "f419f995" "b6709125" "3a04a259");
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,sha1);

} // test
} // beast
} // boost
