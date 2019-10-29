
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test call_if with false condition and void result type.

#include "../detail/oteststream.hpp"
#include <boost/contract/call_if.hpp>
#include <boost/bind.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct eq {
    typedef void result_type; // Test void result type.

    template<typename L, typename R>
    result_type operator()(L left, R right) const {
        out << (left == right) << std::endl; // Requires operator==.
    }
};

struct x {}; // Doest not have operator==.

int main() {
    std::ostringstream ok;
    x x1, x2;;
    
    out.str("");
    boost::contract::call_if<boost::has_equal_to<x> >(
        boost::bind(eq(), x1, x2) // Compiler-error... but not called.
    ); // Test no else.
    ok.str("");
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    // Test "..._c".
    boost::contract::call_if_c<boost::has_equal_to<x>::value>(
        boost::bind(eq(), x1, x2) // Compiler-error...but not called.
    ).else_(
        [] { out << true << std::endl; } // Test else.
    );
    ok.str(""); ok << true << std::endl;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

