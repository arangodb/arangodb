
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test STL equal_to with call_if.

#include "../detail/oteststream.hpp"
#include <boost/contract/call_if.hpp>
#include <boost/bind.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <functional>
#include <sstream>

boost::contract::test::detail::oteststream out;

template<typename T>
struct void_equal_to {
    typedef void result_type; // Test void result type.

    void operator()(T const& left, T const& right) const {
        out << (left == right) << std::endl;
    }
};

struct x {}; // Doest not have operator==.

int main() {
    std::ostringstream ok;
    x x1, x2;;

    out.str("");
    out << // Test on true condition with non-void result type.
        boost::contract::call_if<boost::has_equal_to<int> >(
            boost::bind(std::equal_to<int>(), 123, 123) // True.
        ).else_(
            // Compiler-error... but not called.
            boost::bind(std::equal_to<x>(), x1, x2)
        )
    << std::endl;
    ok.str(""); ok << true << std::endl;
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    out << // Test on false condition with non-void result type.
        boost::contract::call_if<boost::has_equal_to<x> >(
            // Compiler-error... but not called.
            boost::bind(std::equal_to<x>(), x1, x2)
        ).else_([] { return true; })
    << std::endl;
    ok.str(""); ok << true << std::endl;
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    // Test on true condition void result type.
    boost::contract::call_if<boost::has_equal_to<int> >(
        boost::bind(void_equal_to<int>(), 123, 456) // False.
    ).else_(
        // Compiler-error... but not called.
        boost::bind(void_equal_to<x>(), x1, x1)
    );
    ok.str(""); ok << false << std::endl;
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    // Test on false condition with void result type.
    boost::contract::call_if<boost::has_equal_to<x> >(
        // Compiler-error... but not called.
        boost::bind(void_equal_to<x>(), x1, x1)
    ).else_(
        boost::bind(void_equal_to<int>(), 123, 456) // False.
    );
    ok.str(""); ok << false << std::endl;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

