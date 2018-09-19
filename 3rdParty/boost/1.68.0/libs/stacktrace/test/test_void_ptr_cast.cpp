// Copyright 2017 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/stacktrace/detail/void_ptr_cast.hpp>

#include <boost/core/lightweight_test.hpp>

int foo1_func(int) { return 0; }
void foo2_func(int, int, ...) {}

struct test_struct {
    int foo1_memb(int) const { return 0; }
    void foo2_memb(int, int, ...) {}
};

template <class F1, class F2>
void test(F1 foo1, F2 foo2) {
    using boost::stacktrace::detail::void_ptr_cast;

    typedef void(*void_f_ptr)();

    // Function/variable to void(*)()
    void_f_ptr fp1 = void_ptr_cast<void_f_ptr>(foo1);
    void_f_ptr fp2 = void_ptr_cast<void_f_ptr>(foo2);
    BOOST_TEST(fp1);
    BOOST_TEST(fp2);
    BOOST_TEST(fp1 != fp2);

    // Function/variable to void*
    void* vp1 = void_ptr_cast<void*>(foo1);
    void* vp2 = void_ptr_cast<void*>(foo2);
    BOOST_TEST(vp1);
    BOOST_TEST(vp2);
    BOOST_TEST(vp1 != vp2);

    // void* to void(*)()
    void_f_ptr fp1_2 = void_ptr_cast<void_f_ptr>(vp1);
    void_f_ptr fp2_2 = void_ptr_cast<void_f_ptr>(vp2);
    BOOST_TEST(fp1_2);
    BOOST_TEST(fp2_2);
    BOOST_TEST(fp1_2 != fp2_2);
    BOOST_TEST(fp1 == fp1_2);
    BOOST_TEST(fp2 == fp2_2);

    // void(*)() to void*
    BOOST_TEST(void_ptr_cast<void*>(fp1) == vp1);
    BOOST_TEST(void_ptr_cast<void*>(fp2) == vp2);

    // void(*)() to function/variable
    BOOST_TEST(void_ptr_cast<F1>(fp1) == foo1);
    BOOST_TEST(void_ptr_cast<F2>(fp2) == foo2);

    // void* to function/variable
    BOOST_TEST(void_ptr_cast<F1>(vp1) == foo1);
    BOOST_TEST(void_ptr_cast<F2>(vp2) == foo2);
}

int main() {
    // Testing for functions
    test(foo1_func, foo2_func);

    typedef void(func_t)();
    test(
        boost::stacktrace::detail::void_ptr_cast<func_t* const>(foo1_func),
        boost::stacktrace::detail::void_ptr_cast<func_t* const>(foo2_func)
    );

    // Testing for variables (just in case...)
    int i = 0;
    double j= 1;
    test(&i, &j);



    return boost::report_errors();
}
