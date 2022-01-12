// Copyright (c) 2017-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/config.hpp"
#include "boost/variant.hpp"

struct foo {};

struct some_user_provided_visitor_for_lvalues: boost::static_visitor<void> {
    void operator()(foo& ) const {}
    void operator()(int ) const {}
};

int main() {
    boost::apply_visitor(
        some_user_provided_visitor_for_lvalues(),
        boost::variant<int, foo>(foo())
    );

#ifdef __GNUC__
#   if __GNUC__ < 5 && __GNUC_MINOR__ < 8
#       error This test does not pass on GCC < 4.8 because of the incomplete C++11 support
#   endif
#endif

#ifdef BOOST_MSVC
#   error Temporaries/rvalues could bind to non-const lvalues on MSVC compilers
#endif
}
