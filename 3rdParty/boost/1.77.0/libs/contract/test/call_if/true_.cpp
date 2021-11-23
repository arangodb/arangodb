
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test call_if with true condition and non-void result type.

#include "../detail/oteststream.hpp"
#include <boost/contract/call_if.hpp>
#include <boost/bind.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <ios>

boost::contract::test::detail::oteststream out;

struct eq {
    typedef bool result_type; // Test non-void result type.

    template<typename L, typename R>
    result_type operator()(L left, R right) const {
        return left == right; // Requires operator==.
    }
};

struct x {}; // Doest not have operator==.

int main() {
    std::ostringstream ok;
    ok << std::boolalpha;
    out << std::boolalpha;
    x x1, x2;;
    
    out.str("");
    out <<
        boost::contract::call_if<boost::has_equal_to<int> >(
            boost::bind(eq(), 123, 456) // False.
        ) // Test no else (not called).
    << std::endl;
    ok.str(""); ok << false << std::endl;
    BOOST_TEST(out.eq(ok.str()));

    out.str("");
    out <<
        boost::contract::call_if<boost::has_equal_to<int> >(
            boost::bind(eq(), 123, 123) // True.
        ).else_([] { return false; }) // Test else not called.
    << std::endl;
    ok.str(""); ok << true << std::endl;
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    out << // Test "..._c".
        boost::contract::call_if_c<boost::has_equal_to<int>::value>(
            boost::bind(eq(), 123, 123) // True.
        ).else_( // Test else not called.
            boost::bind(eq(), x1, x2) // Compiler-error... but not called.
        )
    << std::endl;
    ok.str(""); ok << true << std::endl;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

