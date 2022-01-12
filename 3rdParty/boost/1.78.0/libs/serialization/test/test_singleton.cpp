/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_singleton.cpp

// (C) Copyright 2018 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <iostream>
#include <boost/serialization/singleton.hpp>

#include "test_tools.hpp"

static int i = 0;

struct A {
    int m_id;
    A() : m_id(++i) {}
    ~A(){
        // verify that objects are destroyed in sequence reverse of construction
        if(i-- != m_id) std::terminate();
    }
};

struct B {
    int m_id;
    B() : m_id(++i) {}
    ~B(){
        // verify that objects are destroyed in sequence reverse of construction
        if(i-- != m_id) std::terminate();
    }
};

struct C {
    int m_id;
    C() : m_id(++i) {}
    ~C(){
        // verify that objects are destroyed in sequence reverse of construction
        if(i-- != m_id) std::terminate();
    }
};

struct D {
    int m_id;
    D(){
        // verify that only one object is indeed created
        const C & c = boost::serialization::singleton<C>::get_const_instance();
        const C & c1 = boost::serialization::singleton<C>::get_const_instance();
        BOOST_CHECK_EQUAL(&c, &c1);

        // verify that objects are created in sequence of definition
        BOOST_CHECK_EQUAL(c.m_id, 1);
        const B & b = boost::serialization::singleton<B>::get_const_instance();
        BOOST_CHECK_EQUAL(b.m_id, 2);
        const A & a = boost::serialization::singleton<A>::get_const_instance();
        BOOST_CHECK_EQUAL(a.m_id, 3);
        std::cout << a.m_id << b.m_id << c.m_id << '\n';

        m_id = ++i;
    }
    ~D(){
        // verify that objects are destroyed in sequence reverse of construction
        if(i-- != m_id) std::terminate();
    }
};

int test_main(int, char *[]){
    return 0;
}

// note: not a singleton
D d;
