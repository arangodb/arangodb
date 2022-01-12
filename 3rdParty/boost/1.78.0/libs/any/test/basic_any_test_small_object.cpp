// Copyright Ruslan Arutyunyan, 2019-2021.
// Copyright Antony Polukhin, 2021.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/any/basic_any.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cassert>

static int move_ctors_count = 0;
static int destructors_count = 0;

struct A {
    char a[24];

    A() {}
    A(const A&) {}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    A(A&&) BOOST_NOEXCEPT {
        ++move_ctors_count;
    }
#endif

    ~A() {
        ++destructors_count;
    }
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template <class T>
    T&& portable_move(T& value) {
        return std::move(value);
    }
#else
    template <class T>
    T& portable_move(T& value) {
        return value;
    }
#endif


int main() {
#if !defined(__GNUC__) || __GNUC__ > 4
    {
        A a;
        boost::anys::basic_any<24, 8> any1(a);
        boost::anys::basic_any<24, 8> any2(portable_move(any1));
        boost::anys::basic_any<24, 8> any3(portable_move(any2));
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_CXX11_NOEXCEPT)
        BOOST_TEST_EQ(move_ctors_count, 2);
#else
        BOOST_TEST_EQ(move_ctors_count, 0);
#endif
    }

#if defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
    // The move constructor is not marked with noexcept, so the large_manager is used.
    // Moving large_manager data is just swapping pointers without calling destructors.
    BOOST_TEST_EQ(destructors_count, 2);
#else
    BOOST_TEST_EQ(destructors_count, 4);
#endif

#endif  // #if !defined(__GNUC__) || __GNUC__ > 4
    return boost::report_errors();
}
