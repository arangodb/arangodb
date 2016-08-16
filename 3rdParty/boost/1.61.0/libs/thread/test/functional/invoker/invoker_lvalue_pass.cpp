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


// <boost/thread/detail/invoker.hpp>

#if ! defined  BOOST_NO_CXX11_DECLTYPE
//#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/thread/detail/async_func.hpp>
#include <boost/detail/lightweight_test.hpp>

int count = 0;

// 1 arg, return void

void f_void_1(int i)
{
    count += i;
}

struct A_void_1
{
    typedef void result_type;
    void operator()(int i)
    {
        count += i;
    }

    void mem1() {
      std::cout << "mem1 " << count << std::endl;
      ++count;
      std::cout << "mem1 " << count << std::endl;
    }
    void mem2() const {count += 2;}
};

void
test_void_1()
{
    int save_count = count;
    // function
#if defined BOOST_THREAD_PROVIDES_INVOKE
    {
    int i = 2;
    boost::detail::invoker<void(*)(int), int>(f_void_1, i)();
    BOOST_TEST(count == save_count + 2);
    save_count = count;
    }
#endif
//    {
//    int i = 2;
//    boost::detail::invoke<void>(f_void_1, i);
//    BOOST_TEST(count == save_count + 2);
//    save_count = count;
//    }
    // function pointer
#if defined BOOST_THREAD_PROVIDES_INVOKE
    {
    void (*fp)(int) = f_void_1;
    int i = 3;
    boost::detail::invoker<void(*)(int), int>(fp, i)();
    BOOST_TEST(count == save_count+3);
    save_count = count;
    }
#endif
//    {
//    void (*fp)(int) = f_void_1;
//    int i = 3;
//    boost::detail::invoke<void>(fp, i);
//    BOOST_TEST(count == save_count+3);
//    save_count = count;
//    }
#if defined BOOST_THREAD_PROVIDES_INVOKE
    {
    void (*fp)(int) = f_void_1;
    int i = 3;
    boost::detail::invoker<void(*)(int), int>(fp, i)();
    BOOST_TEST(count == save_count+3);
    save_count = count;
    }
#endif
//    {
//    void (*fp)(int) = f_void_1;
//    int i = 3;
//    boost::detail::invoke<void>(fp, i);
//    BOOST_TEST(count == save_count+3);
//    save_count = count;
//    }
    // functor
#if defined BOOST_THREAD_PROVIDES_INVOKE
    {
    A_void_1 a0;
    int i = 4;
    boost::detail::invoker<A_void_1, int>(a0, i)();
    BOOST_TEST(count == save_count+4);
    save_count = count;
    }
#endif
//    {
//    A_void_1 a0;
//    int i = 4;
//    boost::detail::invoke<void>(a0, i);
//    BOOST_TEST(count == save_count+4);
//    save_count = count;
//    }
    // member function pointer
#if defined BOOST_THREAD_PROVIDES_INVOKE
    {
    void (A_void_1::*fp)() = &A_void_1::mem1;
    A_void_1 a;
    //BUG
    boost::detail::invoker<void (A_void_1::*)(), A_void_1>(fp, a)();
    BOOST_TEST_EQ(count, save_count+1);
    save_count = count;
    A_void_1* ap = &a;
    boost::detail::invoker<void (A_void_1::*)(), A_void_1*>(fp, ap)();
    BOOST_TEST_EQ(count, save_count+1);
    save_count = count;
    }
#endif
//    {
//    void (A_void_1::*fp)() = &A_void_1::mem1;
//    A_void_1 a;
//    boost::detail::invoke<void>(fp, a);
//    BOOST_TEST(count == save_count+1);
//    save_count = count;
//    A_void_1* ap = &a;
//    boost::detail::invoke<void>(fp, ap);
//    BOOST_TEST(count == save_count+1);
//    save_count = count;
//    }
    // const member function pointer
#if defined BOOST_THREAD_PROVIDES_INVOKE
    {
    void (A_void_1::*fp)() const = &A_void_1::mem2;
    A_void_1 a;
    boost::detail::invoker<void (A_void_1::*)() const, A_void_1>(fp, a)();
    BOOST_TEST(count == save_count+2);
    save_count = count;
    A_void_1* ap = &a;
    boost::detail::invoker<void (A_void_1::*)() const, A_void_1*>(fp, ap)();
    BOOST_TEST_EQ(count, save_count+2);
    save_count = count;
    }
#endif
//    {
//    void (A_void_1::*fp)() const = &A_void_1::mem2;
//    A_void_1 a;
//    boost::detail::invoke<void>(fp, a);
//    BOOST_TEST(count == save_count+2);
//    save_count = count;
//    A_void_1* ap = &a;
//    boost::detail::invoke<void>(fp, ap);
//    BOOST_TEST(count == save_count+2);
//    save_count = count;
//    }
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
    int i = 2;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<int(*)(int), int>(f_int_1, i)() == 3));
#endif
//    BOOST_TEST(boost::detail::invoke<int>(f_int_1, i) == 3);
    }
    // function pointer
    {
    int (*fp)(int) = f_int_1;
    int i = 3;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<int (*)(int), int>(fp, i)() == 4));
#endif
//    BOOST_TEST(boost::detail::invoke<int>(fp, i) == 4);
    }
    // functor
    {
    int i = 4;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<A_int_1, int>(A_int_1(), i)() == 3));
#endif
//    const A_int_1 ca;
//    A_int_1 a;
//    BOOST_TEST(boost::detail::invoke<int>(a, i) == 3);
//    //BOOST_TEST(boost::detail::invoke<int>(ca, i) == 3);
//#if defined BOOST_THREAD_PROVIDES_INVOKE
//    BOOST_TEST(boost::detail::invoke<int>(A_int_1(), i) == 3);
//#endif
    }
    // member function pointer
    {
    A_int_1 a;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<int (A_int_1::*)(), A_int_1>(&A_int_1::mem1, a)() == 3));
#endif
//    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::mem1, a) == 3);
    A_int_1* ap = &a;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<int (A_int_1::*)(), A_int_1*>(&A_int_1::mem1, ap)() == 3));
#endif
//    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::mem1, ap) == 3);
    }
    // const member function pointer
    {
    A_int_1 a;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<int (A_int_1::*)() const, A_int_1>(&A_int_1::mem2, A_int_1())() == 4));
#endif
//    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::mem2, A_int_1()) == 4);
    A_int_1* ap = &a;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    BOOST_TEST((boost::detail::invoker<int (A_int_1::*)() const, A_int_1*>(&A_int_1::mem2, ap)() == 4));
#endif
//    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::mem2, ap) == 4);
    }
    // member data pointer
    {
    A_int_1 a;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    // BUG
    //BOOST_TEST(boost::detail::invoker<int A_int_1::*>(&A_int_1::data_, a) == 5);

//    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::data_, a) == 5);

#endif
//#if defined BOOST_THREAD_PROVIDES_INVOKE
//    A_int_1* ap = &a;
//
//    boost::detail::invoker<int A_int_1::*>(&A_int_1::data_, a) = 6;
//    BOOST_TEST(boost::detail::invoker<int A_int_1::*>(&A_int_1::data_, a) == 6);
//
////    boost::detail::invoke<int>(&A_int_1::data_, a) = 6;
////    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::data_, a) == 6);
//
//#endif
//
//#if defined BOOST_THREAD_PROVIDES_INVOKE
//    BOOST_TEST(boost::detail::invoker<int A_int_1::*>(&A_int_1::data_, ap) == 6);
//    boost::detail::invoker<int A_int_1::*>(&A_int_1::data_, ap) = 7;
//    BOOST_TEST(boost::detail::invoker<int A_int_1::*>(&A_int_1::data_, ap) == 7);
//
////    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::data_, ap) == 7);
////    boost::detail::invoke<int>(&A_int_1::data_, ap) = 8;
////    BOOST_TEST(boost::detail::invoke<int>(&A_int_1::data_, ap) == 8);
//#endif

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
    int i = 2;
    int j = 3;
#if defined BOOST_THREAD_PROVIDES_INVOKE
    boost::detail::invoke(f_void_2, i, j);
    BOOST_TEST(count == save_count+5);
    save_count = count;
#endif
//    boost::detail::invoke<void>(f_void_2, i, j);
//    BOOST_TEST(count == save_count+5);
//    save_count = count;
    }
    // member function pointer
    {
#if defined BOOST_THREAD_PROVIDES_INVOKE
      int j = 3;
    boost::detail::invoke(&A_void_2::mem1, A_void_2(), j);
    BOOST_TEST(count == save_count+3);
    save_count = count;

//    boost::detail::invoke<void>(&A_void_2::mem1, A_void_2(), j);
//    BOOST_TEST(count == save_count+3);
//    save_count = count;
#endif
//    A_void_2 a2;
//    boost::detail::invoke<void>(&A_void_2::mem1, a2, j);
//    BOOST_TEST(count == save_count+3);
//    save_count = count;
    }
}

int main()
{
    test_void_1();
    test_int_1();
    test_void_2();
    return boost::report_errors();

}
