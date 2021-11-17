// Special tests for erase_begin, erase_end and clear methods.

// Copyright (c) 2009 Jan Gaspar

// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"

int MyInteger::ms_exception_trigger = 0;
int InstanceCounter::ms_count = 0;

void erase_begin_test() {

    circular_buffer<int> cb1(5);
    cb1.push_back(1);
    cb1.push_back(2);
    cb1.push_back(3);
    cb1.push_back(4);
    cb1.push_back(5);
    cb1.push_back(6);

    circular_buffer<int>::pointer p = &cb1[0];

    cb1.erase_begin(2);

    BOOST_TEST(cb1.size() == 3);
    BOOST_TEST(cb1[0] == 4);
    BOOST_TEST(cb1[1] == 5);
    BOOST_TEST(cb1[2] == 6);

    cb1.erase_begin(3);
    BOOST_TEST(cb1.empty());
    BOOST_TEST(*p == 2);
    BOOST_TEST(*(p + 1) == 3);
    BOOST_TEST(*(p + 2) == 4);

    cb1.push_back(10);
    cb1.push_back(11);
    cb1.push_back(12);

    BOOST_TEST(cb1.size() == 3);
    BOOST_TEST(cb1[0] == 10);
    BOOST_TEST(cb1[1] == 11);
    BOOST_TEST(cb1[2] == 12);

    circular_buffer<InstanceCounter> cb2(5, InstanceCounter());

    BOOST_TEST(cb2.size() == 5);
    BOOST_TEST(InstanceCounter::count() == 5);

    cb2.erase_begin(2);

    BOOST_TEST(cb2.size() == 3);
    BOOST_TEST(InstanceCounter::count() == 3);

    circular_buffer<MyInteger> cb3(5);
    cb3.push_back(1);
    cb3.push_back(2);
    cb3.push_back(3);
    cb3.push_back(4);
    cb3.push_back(5);
    cb3.push_back(6);
    cb3.erase_begin(2);

    BOOST_TEST(cb3.size() == 3);
    BOOST_TEST(cb3[0] == 4);
    BOOST_TEST(cb3[1] == 5);
    BOOST_TEST(cb3[2] == 6);
}

void erase_end_test() {

    circular_buffer<int> cb1(5);
    cb1.push_back(1);
    cb1.push_back(2);
    cb1.push_back(3);
    cb1.push_back(4);
    cb1.push_back(5);
    cb1.push_back(6);

    circular_buffer<int>::pointer p = &cb1[3];

    cb1.erase_end(2);

    BOOST_TEST(cb1.size() == 3);
    BOOST_TEST(cb1[0] == 2);
    BOOST_TEST(cb1[1] == 3);
    BOOST_TEST(cb1[2] ==4);

    cb1.erase_end(3);
    BOOST_TEST(cb1.empty());
    BOOST_TEST(*p == 5);
    BOOST_TEST(*(p - 1) == 4);
    BOOST_TEST(*(p - 2) == 3);

    cb1.push_back(10);
    cb1.push_back(11);
    cb1.push_back(12);

    BOOST_TEST(cb1.size() == 3);
    BOOST_TEST(cb1[0] == 10);
    BOOST_TEST(cb1[1] == 11);
    BOOST_TEST(cb1[2] == 12);

    circular_buffer<InstanceCounter> cb2(5, InstanceCounter());

    BOOST_TEST(cb2.size() == 5);
    BOOST_TEST(InstanceCounter::count() == 5);

    cb2.erase_end(2);

    BOOST_TEST(cb2.size() == 3);
    BOOST_TEST(InstanceCounter::count() == 3);

    circular_buffer<MyInteger> cb3(5);
    cb3.push_back(1);
    cb3.push_back(2);
    cb3.push_back(3);
    cb3.push_back(4);
    cb3.push_back(5);
    cb3.push_back(6);
    cb3.erase_end(2);

    BOOST_TEST(cb3.size() == 3);
    BOOST_TEST(cb3[0] == 2);
    BOOST_TEST(cb3[1] == 3);
    BOOST_TEST(cb3[2] == 4);
}

void clear_test() {

    circular_buffer<int> cb1(5);
    cb1.push_back(1);
    cb1.push_back(2);
    cb1.push_back(3);
    cb1.push_back(4);
    cb1.push_back(5);
    cb1.push_back(6);

    circular_buffer<int>::pointer p = &cb1[0];

    cb1.clear();

    BOOST_TEST(cb1.empty());
    BOOST_TEST(*p == 2);
    BOOST_TEST(*(p + 1) == 3);
    BOOST_TEST(*(p + 2) == 4);
    BOOST_TEST(*(p + 3) == 5);
    BOOST_TEST(*(p - 1) == 6);

    circular_buffer<InstanceCounter> cb2(5, InstanceCounter());

    BOOST_TEST(cb2.size() == 5);
    BOOST_TEST(InstanceCounter::count() == 5);

    cb2.clear();

    BOOST_TEST(cb2.empty());
    BOOST_TEST(InstanceCounter::count() == 0);

    circular_buffer<MyInteger> cb3(5);
    cb3.push_back(1);
    cb3.push_back(2);
    cb3.push_back(3);
    cb3.push_back(4);
    cb3.push_back(5);
    cb3.push_back(6);
    cb3.clear();

    BOOST_TEST(cb3.empty());
}

// test main
int main()
{
    erase_begin_test();
    erase_end_test();
    clear_test();
    return boost::report_errors();
}
