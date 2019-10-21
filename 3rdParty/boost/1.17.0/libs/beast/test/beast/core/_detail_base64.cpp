//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/base64.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <string>

namespace boost {
namespace beast {
namespace detail {

class base64_test : public beast::unit_test::suite
{
public:
    std::string
    base64_encode(
        std::uint8_t const* data,
        std::size_t len)
    {
        std::string dest;
        dest.resize(base64::encoded_size(len));
        dest.resize(base64::encode(&dest[0], data, len));
        return dest;
    }

    std::string
    base64_encode(string_view s)
    {
        return base64_encode (reinterpret_cast <
            std::uint8_t const*> (s.data()), s.size());
    }

    std::string
    base64_decode(string_view data)
    {
        std::string dest;
        dest.resize(base64::decoded_size(data.size()));
        auto const result = base64::decode(
            &dest[0], data.data(), data.size());
        dest.resize(result.first);
        return dest;
    }

    void
    check (std::string const& in, std::string const& out)
    {
        auto const encoded = base64_encode (in);
        BEAST_EXPECT(encoded == out);
        BEAST_EXPECT(base64_decode (encoded) == in);
    }

    void
    run()
    {
        check ("",       "");
        check ("f",      "Zg==");
        check ("fo",     "Zm8=");
        check ("foo",    "Zm9v");
        check ("foob",   "Zm9vYg==");
        check ("fooba",  "Zm9vYmE=");
        check ("foobar", "Zm9vYmFy");

        check(
            "Man is distinguished, not only by his reason, but by this singular passion from "
            "other animals, which is a lust of the mind, that by a perseverance of delight "
            "in the continued and indefatigable generation of knowledge, exceeds the short "
            "vehemence of any carnal pleasure."
            ,
            "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
            "IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
            "dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
            "dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
            "ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4="
            );
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,base64);

} // detail
} // beast
} // boost
