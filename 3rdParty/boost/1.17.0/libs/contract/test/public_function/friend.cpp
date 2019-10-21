
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test friend functions (also forcing them to check invariants).

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

class y;
class z;

class x {
public:
    void invariant() const {
        out << "x::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(get() >= 0);
    }

    x() : value_(0) {}
    int get() const { return value_; }

    friend void f(x&, y&, int value);

private:
    int value_;
};

class y {
public:
    void invariant() const {
        out << "y::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(get() >= 0);
    }

    y() : value_(0) {}
    int get() const { return value_; }

    friend void f(x&, y&, int value);

private:
    int value_;
};

void f(x& a, y& b, int value) {
    boost::contract::check post = boost::contract::function()
        .postcondition([&] {
            out << "f::post" << std::endl;
            BOOST_CONTRACT_ASSERT(a.get() == value);
            BOOST_CONTRACT_ASSERT(b.get() == value);
        })
    ;
    boost::contract::check inv_b = boost::contract::public_function(&b);
    boost::contract::check inv_a = boost::contract::public_function(&a);
    boost::contract::check pre = boost::contract::function()
        .precondition([&] {
            out << "f::pre" << std::endl;
            BOOST_CONTRACT_ASSERT(value > 0);
        })
    ;

    out << "f::body" << std::endl;
    a.value_ = b.value_ = value;
}

int main() {
    std::ostringstream ok;

    x a;
    y b;
    
    out.str("");
    f(a, b, 123); 
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "y::inv" << std::endl
            << "x::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "f::pre" << std::endl
        #endif
        << "f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "x::inv" << std::endl
            << "y::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    BOOST_TEST_EQ(a.get(), 123);
    BOOST_TEST_EQ(b.get(), 123);

    return boost::report_errors();
}

