/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_shared_ptr_multi_base.cpp

// (C) Copyright 2002 Robert Ramey- http://www.rrsd.com and Takatoshi Kondo.
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org for updates, documentation, and revision history.

#include <cstddef> // NULL
#include <cstdio> // remove
#include <fstream>

#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/export.hpp>

#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/weak_ptr.hpp>

#include "test_tools.hpp"

struct Base1 {
    Base1() {}
    Base1(int x) : m_x(1 + x) {}
    virtual ~Base1(){
    }
    int m_x;
    // serialize
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /* file_version */)
    {
        ar & BOOST_SERIALIZATION_NVP(m_x);
    }
};

struct Base2 {
    Base2() {}
    Base2(int x) : m_x(2 + x) {}
    int m_x;
    virtual ~Base2(){
    }
    // serialize
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /* file_version */)
    {
        ar & BOOST_SERIALIZATION_NVP(m_x);
    }
};

struct Base3 {
    Base3() {}
    Base3(int x) : m_x(3 + x) {}
    virtual ~Base3(){
    }
    int m_x;
    // serialize
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /* file_version */)
    {
        ar & BOOST_SERIALIZATION_NVP(m_x);
    }
};

// Sub is a subclass of Base1, Base1 and Base3.
struct Sub:public Base1, public Base2, public Base3 {
    static int count;
    Sub() {
        ++count;
    }
    Sub(int x) :
        Base1(x),
        Base2(x),
        m_x(x)
    {
        ++count;
    }
    Sub(const Sub & rhs) :
        m_x(rhs.m_x)
    {
        ++count;
    }
    virtual ~Sub() {
        assert(0 < count);
        --count;
    }
    int m_x;
    // serialize
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /* file_version */)
    {
        ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base1);
        ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base2);
        ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base3);
        ar & BOOST_SERIALIZATION_NVP(m_x);
    }
};

// Sub needs to be exported because its serialized via a base class pointer
BOOST_CLASS_EXPORT(Sub)
BOOST_SERIALIZATION_SHARED_PTR(Sub)

int Sub::count = 0;

template <class FIRST, class SECOND>
void save2(
    const char * testfile,
    const FIRST& first,
    const SECOND& second
){
    test_ostream os(testfile, TEST_STREAM_FLAGS);
    test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
    oa << BOOST_SERIALIZATION_NVP(first);
    oa << BOOST_SERIALIZATION_NVP(second);
}

template <class FIRST, class SECOND>
void load2(
    const char * testfile,
    FIRST& first,
    SECOND& second)
{
    test_istream is(testfile, TEST_STREAM_FLAGS);
    test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
    ia >> BOOST_SERIALIZATION_NVP(first);
    ia >> BOOST_SERIALIZATION_NVP(second);
}

// Run tests by serializing two shared_ptrs into an archive,
// clearing them (deleting the objects) and then reloading the
// objects back from an archive.

template<class T, class U>
boost::shared_ptr<T> dynamic_pointer_cast(boost::shared_ptr<U> const & u)
BOOST_NOEXCEPT
{
    return boost::dynamic_pointer_cast<T>(u);
}

#ifndef BOOST_NO_CXX11_SMART_PTR
template<class T, class U>
std::shared_ptr<T> dynamic_pointer_cast(std::shared_ptr<U> const & u)
BOOST_NOEXCEPT
{
    return std::dynamic_pointer_cast<T>(u);
}
#endif

// Serialization sequence
// First,  shared_ptr
// Second, weak_ptr
template <class SP, class WP>
void shared_weak(
    SP & first,
    WP & second
){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);
    int firstm = first->m_x;

    BOOST_REQUIRE(! second.expired());
    int secondm = second.lock()->m_x;
    save2(testfile, first, second);

    // Clear the pointers, thereby destroying the objects they contain
    second.reset();
    first.reset();

    load2(testfile, first, second);
    BOOST_CHECK(! second.expired());

    // Check data member
    BOOST_CHECK(firstm == first->m_x);
    BOOST_CHECK(secondm == second.lock()->m_x);
    // Check pointer to vtable
    BOOST_CHECK(::dynamic_pointer_cast<Sub>(first));
    BOOST_CHECK(::dynamic_pointer_cast<Sub>(second.lock()));

    std::remove(testfile);
}

// Serialization sequence
// First,  weak_ptr
// Second, shared_ptr
template <class WP, class SP>
void weak_shared(
    WP & first,
    SP & second
){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);
    BOOST_CHECK(! first.expired());
    int firstm = first.lock()->m_x;
    int secondm = second->m_x;
    save2(testfile, first, second);

    // Clear the pointers, thereby destroying the objects they contain
    first.reset();
    second.reset();

    load2(testfile, first, second);
    BOOST_CHECK(! first.expired());

    // Check data member
    BOOST_CHECK(firstm == first.lock()->m_x);
    BOOST_CHECK(secondm == second->m_x);
    // Check pointer to vtable
    BOOST_CHECK(::dynamic_pointer_cast<Sub>(first.lock()));
    BOOST_CHECK(::dynamic_pointer_cast<Sub>(second));

    std::remove(testfile);
}

// This does the tests
template<template<class T> class SPT, template<class T> class WPT>
bool test(){
    // Both Sub
    SPT<Sub> tc1_sp(new Sub(10));
    WPT<Sub> tc1_wp(tc1_sp);
    shared_weak(tc1_sp, tc1_wp);
    weak_shared(tc1_wp, tc1_sp);
    tc1_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    // Sub and Base1
    SPT<Sub> tc2_sp(new Sub(10));
    WPT<Base1> tc2_wp(tc2_sp);
    shared_weak(tc2_sp, tc2_wp);
    weak_shared(tc2_wp, tc2_sp);
    tc2_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    // Sub and Base2
    SPT<Sub> tc3_sp(new Sub(10));
    WPT<Base2> tc3_wp(tc3_sp);
    shared_weak(tc3_sp, tc3_wp);
    weak_shared(tc3_wp, tc3_sp);
    tc3_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    // Sub and Base3
    SPT<Sub> tc4_sp(new Sub(10));
    WPT<Base3> tc4_wp(tc4_sp);
    shared_weak(tc4_sp, tc4_wp);
    weak_shared(tc4_wp, tc4_sp);
    tc4_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    // Base1 and Base2
    SPT<Sub> tc5_sp_tmp(new Sub(10));
    SPT<Base1> tc5_sp(tc5_sp_tmp);
    WPT<Base2> tc5_wp(tc5_sp_tmp);
    tc5_sp_tmp.reset();
    shared_weak(tc5_sp, tc5_wp);
    weak_shared(tc5_wp, tc5_sp);
    tc5_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    // Base2 and Base3
    SPT<Sub> tc6_sp_tmp(new Sub(10));
    SPT<Base2> tc6_sp(tc6_sp_tmp);
    WPT<Base3> tc6_wp(tc6_sp_tmp);
    tc6_sp_tmp.reset();
    shared_weak(tc6_sp, tc6_wp);
    weak_shared(tc6_wp, tc6_sp);
    tc6_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    // Base3 and Base1
    SPT<Sub> tc7_sp_tmp(new Sub(10));
    SPT<Base3> tc7_sp(tc7_sp_tmp);
    WPT<Base1> tc7_wp(tc7_sp_tmp);
    tc7_sp_tmp.reset();
    shared_weak(tc7_sp, tc7_wp);
    weak_shared(tc7_wp, tc7_sp);
    tc7_sp.reset();
    BOOST_CHECK(0 == Sub::count);

    return true;
}

// This does the tests
int test_main(int /* argc */, char * /* argv */[])
{
    bool result = true;
    result &= test<boost::shared_ptr, boost::weak_ptr>();
    #ifndef BOOST_NO_CXX11_SMART_PTR
    result &= test<std::shared_ptr, std::weak_ptr>();
    #endif
    return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
