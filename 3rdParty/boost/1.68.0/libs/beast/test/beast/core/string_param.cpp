//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/string_param.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <boost/beast/core/detail/type_traits.hpp>

namespace boost {
namespace beast {

class string_param_test : public unit_test::suite
{
public:
    struct nop {};

    void
    check(string_param const& v, string_view s)
    {
        BEAST_EXPECT(static_cast<string_view>(v) == s);
    }

    class repeater
    {
        std::size_t n_;

    public:
        explicit
        repeater(std::size_t n)
            : n_(n)
        {
        }

        friend
        std::ostream&
        operator<<(std::ostream& os, repeater const& v)
        {
            return os << std::string(v.n_, '*');
        }
    };

    void
    testConversion()
    {
        // Make sure things convert correctly
        check(std::string("hello"), "hello");
        check("xyz", "xyz");
        check(1, "1");
        check(12, "12");
        check(123, "123");
        check(1234, "1234");
        check(12345, "12345");
        check({"a", "b"}, "ab");
        check({1, 2, 3}, "123");
    }

    void
    testStaticOstream()
    {
        // exercise static_ostream for coverage
        std::string s(500, '*');
        check(repeater{500}, s);
    }

    void
    run() override
    {
        testConversion();
        testStaticOstream();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,string_param);

} // beast
} // boost
