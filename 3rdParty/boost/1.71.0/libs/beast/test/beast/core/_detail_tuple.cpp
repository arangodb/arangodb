//
// Copyright (c) 2016-2019Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/tuple.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace detail {

class tuple_test : public beast::unit_test::suite
{
public:
    void
    run()
    {
        struct explicit_constructible
        {
            explicit_constructible(std::nullptr_t)
                : i_(0)
            {
            }

            explicit explicit_constructible(int i)
                : i_(i)
            {
            }

            int i_;
        };

        tuple<explicit_constructible, int> t{nullptr, 42};
        BEAST_EXPECT(detail::get<1>(t) == 42);
        BEAST_EXPECT(detail::get<0>(t).i_ == 0);

        t = tuple<explicit_constructible, int>{explicit_constructible(42), 43};
        BEAST_EXPECT(detail::get<1>(t) == 43);
        BEAST_EXPECT(detail::get<0>(t).i_ == 42);
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,tuple);

} // detail
} // beast
} // boost
