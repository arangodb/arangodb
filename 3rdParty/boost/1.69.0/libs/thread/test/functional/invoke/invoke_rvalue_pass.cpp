//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/detail/invoke.hpp>

#include <boost/thread/detail/invoke.hpp>
#include <boost/detail/lightweight_test.hpp>

int count = 0;

// 1 arg, return void

void f_void_1(int i)
{
    count += i;
}

struct A_void_1
{
    void operator()(int i)
    {
        count += i;
    }

    void mem1() {++count;}
    void mem2() const {count += 2;}
};

void
test_void_1()
{
    int save_count = count;
    // function
    {
    boost::detail::invoke(f_void_1, 2);
    BOOST_TEST(count == save_count + 2);
    save_count = count;
    }
    // function pointer
    {
    void (*fp)(int) = f_void_1;
    boost::detail::invoke(fp, 3);
    BOOST_TEST(count == save_count+3);
    save_count = count;
    }
    // functor
    {
    A_void_1 a0;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    boost::detail::invoke(a0, 4);
    BOOST_TEST(count == save_count+4);
    save_count = count;
#endif
    boost::detail::invoke<void>(a0, 4);
    BOOST_TEST(count == save_count+4);
    save_count = count;
    }
    // member function pointer
    {
#if defined BOOST_THREAD_PROVIDES_INVOKE
    void (A_void_1::*fp)() = &A_void_1::mem1;
    boost::detail::invoke(fp, A_void_1());
    BOOST_TEST(count == save_count+1);
    save_count = count;
    //BUG
    boost::detail::invoke<void>(fp, A_void_1());
    BOOST_TEST(count == save_count+1);
    save_count = count;

#endif
#if defined BOOST_THREAD_PROVIDES_INVOKE
    A_void_1 a;
    boost::detail::invoke(fp, &a);
    BOOST_TEST(count == save_count+1);
    save_count = count;
    //BUG
    boost::detail::invoke<int>(fp, &a);
    BOOST_TEST(count == save_count+1);
    save_count = count;

#endif
    }
    // const member function pointer
    {
    void (A_void_1::*fp)() const = &A_void_1::mem2;
    boost::detail::invoke(fp, A_void_1());
    BOOST_TEST(count == save_count+2);
    save_count = count;
    A_void_1 a;
    boost::detail::invoke(fp, &a);
    BOOST_TEST(count == save_count+2);
    save_count = count;
    }
}

// 1 arg, return int

int f_int_1(int i)
{
    return i + 1;
}

struct A_int_1
{
    A_int_1() : data_(5) {}
    int operator()(int i)
    {
        return i - 1;
    }

    int mem1() {return 3;}
    int mem2() const {return 4;}
    int data_;
};

void
test_int_1()
{
    // function
    {
    BOOST_TEST(boost::detail::invoke(f_int_1, 2) == 3);
    }
    // function pointer
    {
    int (*fp)(int) = f_int_1;
    BOOST_TEST(boost::detail::invoke(fp, 3) == 4);
    }
    // functor
    {
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST(boost::detail::invoke(A_int_1(), 4) == 3);
    BOOST_TEST(boost::detail::invoke<int>(A_int_1(), 4) == 3);
#endif
    }
    // member function pointer
    {
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST(boost::detail::invoke(&A_int_1::mem1, A_int_1()) == 3);
    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::mem1, A_int_1()) == 3);
#endif

    A_int_1 a;
    BOOST_TEST(boost::detail::invoke(&A_int_1::mem1, &a) == 3);
    }
    // const member function pointer
    {
    BOOST_TEST(boost::detail::invoke(&A_int_1::mem2, A_int_1()) == 4);
    A_int_1 a;
    BOOST_TEST(boost::detail::invoke(&A_int_1::mem2, &a) == 4);
    }
    // member data pointer
    {
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST(boost::detail::invoke(&A_int_1::data_, A_int_1()) == 5);
    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::data_, A_int_1()) == 5);
    A_int_1 a;
    BOOST_TEST(boost::detail::invoke(&A_int_1::data_, a) == 5);
    boost::detail::invoke(&A_int_1::data_, a) = 6;
    BOOST_TEST(boost::detail::invoke(&A_int_1::data_, a) == 6);
    BOOST_TEST(boost::detail::invoke(&A_int_1::data_, &a) == 6);
    boost::detail::invoke(&A_int_1::data_, &a) = 7;
    BOOST_TEST(boost::detail::invoke(&A_int_1::data_, &a) == 7);
#endif
    }
}

// 2 arg, return void

void f_void_2(int i, int j)
{
    count += i+j;
}

struct A_void_2
{
    void operator()(int i, int j)
    {
        count += i+j;
    }

    void mem1(int i) {count += i;}
    void mem2(int i) const {count += i;}
};

void
test_void_2()
{
    int save_count = count;
    // function
    {
    boost::detail::invoke(f_void_2, 2, 3);
    BOOST_TEST(count == save_count+5);
    save_count = count;
    }
    // member function pointer
    {
#if defined BOOST_THREAD_PROVIDES_INVOKE
    boost::detail::invoke(&A_void_2::mem1, A_void_2(), 3);
    BOOST_TEST(count == save_count+3);
    save_count = count;

    boost::detail::invoke<void>(&A_void_2::mem1, A_void_2(), 3);
    BOOST_TEST(count == save_count+3);
    save_count = count;
#endif

    }
}

int main()
{
    test_void_1();
    test_int_1();
    test_void_2();
    return boost::report_errors();
}
