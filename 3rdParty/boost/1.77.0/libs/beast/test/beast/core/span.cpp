//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/span.hpp>

#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <vector>

namespace boost {
namespace beast {

class span_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        detail::is_contiguous_container<
        string_view, char const>::value);

    struct base {};
    struct derived : base {};

    BOOST_STATIC_ASSERT(detail::is_contiguous_container<
        std::vector<char>, char>::value);
    
    BOOST_STATIC_ASSERT(detail::is_contiguous_container<
        std::vector<char>, char const>::value);

    BOOST_STATIC_ASSERT(! detail::is_contiguous_container<
        std::vector<derived>, base>::value);

    BOOST_STATIC_ASSERT(! detail::is_contiguous_container<
        std::vector<derived>, base const>::value);

    void
    testSpan()
    {
        span<char const> sp{"hello", 5};
        BEAST_EXPECT(sp.size() == 5);
        std::string s("world");
        sp = s;
    }

    void
    run() override
    {
        testSpan();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,span);

} // beast
} // boost
