
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test free function contracts.

#include "../detail/oteststream.hpp"
#include "../detail/counter.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/old.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/check.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct x_tag; typedef boost::contract::test::detail::counter<x_tag, int> x_type;
struct y_tag; typedef boost::contract::test::detail::counter<y_tag, int> y_type;

bool swap(x_type& x, y_type& y) {
    bool result;
    boost::contract::old_ptr<x_type> old_x =
            BOOST_CONTRACT_OLDOF(x_type::eval(x));
    boost::contract::old_ptr<y_type> old_y;
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            out << "swap::pre" << std::endl;
            BOOST_CONTRACT_ASSERT(x.value != y.value);
        })
        .old([&] {
            out << "swap::old" << std::endl;
            old_y = BOOST_CONTRACT_OLDOF(y_type::eval(y));
        })
        .postcondition([&] {
            out << "swap::post" << std::endl;
            BOOST_CONTRACT_ASSERT(x.value == old_y->value);
            BOOST_CONTRACT_ASSERT(y.value == old_x->value);
            BOOST_CONTRACT_ASSERT(result == (old_x->value != old_y->value));
        })
    ;

    out << "swap::body" << std::endl;
    if(x.value == y.value) return result = false;
    int save_x = x.value;
    x.value = y.value;
    y.value = save_x;
    return result = true;
}

int main() {
    std::ostringstream ok;

    {
        x_type x; x.value = 123;
        y_type y; y.value = 456;

        out.str("");
        bool r = swap(x, y);
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "swap::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "swap::old" << std::endl
            #endif
            << "swap::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "swap::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
        
        BOOST_TEST(r);
        BOOST_TEST_EQ(x.value, 456);
        BOOST_TEST_EQ(y.value, 123);
    }

    #ifndef BOOST_CONTRACT_NO_OLDS
        #define BOOST_CONTRACT_TEST_old 1u
    #else
        #define BOOST_CONTRACT_TEST_old 0u
    #endif

    BOOST_TEST_EQ(x_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(x_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(x_type::ctors(), x_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(y_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(y_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(y_type::ctors(), y_type::dtors()); // No leak.

    #undef BOOST_CONTRACT_TEST_old
    return boost::report_errors();
}

