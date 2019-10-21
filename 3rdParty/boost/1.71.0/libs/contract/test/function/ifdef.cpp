
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off.

#include "../detail/oteststream.hpp"
#include <boost/contract/core/config.hpp>
#ifndef BOOST_CONTRACT_NO_FUNCTIONS
    #include <boost/contract/function.hpp>
    #include <boost/contract/check.hpp>
    #include <boost/contract/old.hpp>
#endif
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

void f(int x) {
    #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
        boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(x);
    #endif
    #ifndef BOOST_CONTRACT_NO_FUNCTIONS
        boost::contract::check c = boost::contract::function()
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                .precondition([] { out << "f::pre" << std::endl; })
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                .old([] { out << "f::old" << std::endl; })
                .postcondition([] { out << "f::post" << std::endl; })
            #endif
        ;
    #endif
    out << "f::body" << std::endl;
}

int main() {
    std::ostringstream ok;
    out.str("");
    f(123);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "f::old" << std::endl
        #endif
        << "f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    return boost::report_errors();
}

