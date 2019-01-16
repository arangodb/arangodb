// Copyright (c) 2017 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/config.hpp"
#include "boost/variant.hpp"
#include <string>

struct some_user_provided_visitor_for_lvalues: boost::static_visitor<void> {
    void operator()(std::string& ) const {}
    void operator()(int ) const {}
};

int main() {
    boost::apply_visitor(
        some_user_provided_visitor_for_lvalues(),
        boost::variant<int, std::string>("Hello")
    );

#ifdef BOOST_MSVC
#   error Temporaries/rvalues could bind to non-const lvalues on MSVC compilers
#endif

    return 0;
}
