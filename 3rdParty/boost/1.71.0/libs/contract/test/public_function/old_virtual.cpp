
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test old inits/ftors and of mixed types up inheritance tree.

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/old.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/override.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

boost::contract::test::detail::oteststream out;

struct num {
    static num make(int i) { // Test no ctor (not even explicit) but for copy.
        num n;
        n.value(i);
        return n;
    }

    num(num const& other) : value_(other.value_) {}

    void value(int i) { value_ = boost::lexical_cast<std::string>(i); }
    int value() const { return boost::lexical_cast<int>(value_); }

    num operator+(int left) {
        num n;
        n.value(value() + left);
        return n;
    }
    
private:
    num() {} // Test no visible default ctor (only copy ctor).
    num& operator=(num const&); // Test no copy operator (only copy ctor).

    std::string value_; // Test this size-of != from other old type `int` below.
};

struct c {
    virtual void f(int& i, num& n, boost::contract::virtual_* v = 0) {
        boost::contract::old_ptr<int> old_a = BOOST_CONTRACT_OLDOF(v, i + 1);
        boost::contract::old_ptr<num> old_b = BOOST_CONTRACT_OLDOF(v, n + 2);
        boost::contract::old_ptr<int> old_x;
        boost::contract::old_ptr<num> old_y;
        boost::contract::check c = boost::contract::public_function(v, this)
            .old([&] {
                out << "c::f::old" << std::endl;
                old_x = BOOST_CONTRACT_OLDOF(v, i + 3);
                old_y = BOOST_CONTRACT_OLDOF(v, n + 4);
            })
            .postcondition([&] {
                out << "c::f::post" << std::endl;
                BOOST_CONTRACT_ASSERT(*old_a == n.value() + 1);
                BOOST_CONTRACT_ASSERT(old_b->value() == i + 2);
                BOOST_CONTRACT_ASSERT(*old_x == n.value() + 3);
                BOOST_CONTRACT_ASSERT(old_y->value() == i + 4);
            })
        ;
        out << "c::f::body" << std::endl;
        int tmp = i;
        i = n.value();
        n.value(tmp);
    }
};

struct b
    #define BASES public c
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    virtual void f(int& i, num& n, boost::contract::virtual_* v = 0)
            /* override */ {
        boost::contract::old_ptr<int> old_a = BOOST_CONTRACT_OLDOF(v, i + 1);
        boost::contract::old_ptr<num> old_b = BOOST_CONTRACT_OLDOF(v, n + 2);
        boost::contract::old_ptr<int> old_x;
        boost::contract::old_ptr<num> old_y;
        boost::contract::check c = boost::contract::public_function<
                override_f>(v, &c::f, this, i, n)
            .old([&] {
                out << "b::f::old" << std::endl;
                old_x = BOOST_CONTRACT_OLDOF(v, i + 3);
                old_y = BOOST_CONTRACT_OLDOF(v, n + 4);
            })
            .postcondition([&] {
                out << "b::f::post" << std::endl;
                BOOST_CONTRACT_ASSERT(*old_a == n.value() + 1);
                BOOST_CONTRACT_ASSERT(old_b->value() == i + 2);
                BOOST_CONTRACT_ASSERT(*old_x == n.value() + 3);
                BOOST_CONTRACT_ASSERT(old_y->value() == i + 4);
            })
        ;
        out << "b::f::body" << std::endl;
        int tmp = i;
        i = n.value();
        n.value(tmp);
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    virtual void f(int& i, num& n, boost::contract::virtual_* v = 0)
            /* override */ {
        boost::contract::old_ptr<int> old_a = BOOST_CONTRACT_OLDOF(v, i + 1);
        boost::contract::old_ptr<num> old_b = BOOST_CONTRACT_OLDOF(v, n + 2);
        boost::contract::old_ptr<int> old_x;
        boost::contract::old_ptr<num> old_y;
        boost::contract::check c = boost::contract::public_function<
                override_f>(v, &c::f, this, i, n)
            .old([&] {
                out << "a::f::old" << std::endl;
                old_x = BOOST_CONTRACT_OLDOF(v, i + 3);
                old_y = BOOST_CONTRACT_OLDOF(v, n + 4);
            })
            .postcondition([&] {
                out << "a::f::post" << std::endl;
                BOOST_CONTRACT_ASSERT(*old_a == n.value() + 1);
                BOOST_CONTRACT_ASSERT(old_b->value() == i + 2);
                BOOST_CONTRACT_ASSERT(*old_x == n.value() + 3);
                BOOST_CONTRACT_ASSERT(old_y->value() == i + 4);
            })
        ;
        out << "a::f::body" << std::endl;
        int tmp = i;
        i = n.value();
        n.value(tmp);
    }
    BOOST_CONTRACT_OVERRIDE(f)
};
    
int main() {
    std::ostringstream ok;
    int i = 0;
    num n = num::make(0);

    i = 123;
    n.value(-123);
    a aa; // Test virtual call with 2 bases.
    out.str("");
    aa.f(i, n);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::f::old" << std::endl
            << "b::f::old" << std::endl
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::f::old" << std::endl
            << "c::f::post" << std::endl
            << "b::f::old" << std::endl
            << "b::f::post" << std::endl
            // No old call here because not a base object.
            << "a::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    i = 456;
    n.value(-456);
    b bb; // Test virtual call with 1 base.
    out.str("");
    bb.f(i, n);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::f::old" << std::endl
            << "b::f::old" << std::endl
        #endif
        << "b::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::f::old" << std::endl
            << "c::f::post" << std::endl
            // No old call here because not a base object.
            << "b::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    i = 789;
    n.value(-789);
    c cc; // Test virtual call with no bases.
    out.str("");
    cc.f(i, n);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::f::old" << std::endl
        #endif
        << "c::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            // No old call here because not a base object.
            << "c::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

