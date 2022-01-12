/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_list.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef>
#include <fstream>

#include <boost/config.hpp>
#include <cstdio> // remove
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif

#include <boost/type_traits/is_pointer.hpp>
#include <boost/static_assert.hpp>
#include <boost/checked_delete.hpp>

#include <boost/archive/archive_exception.hpp>
#include "test_tools.hpp"

#include <boost/serialization/nvp.hpp>

#include "A.hpp"
#include "A.ipp"

template<class T>
struct ptr_equal_to {
    BOOST_STATIC_ASSERT(::boost::is_pointer< T >::value);
    bool operator()(T const _Left, T const _Right) const
    {
        if(NULL == _Left && NULL == _Right)
            return true;
        if(typeid(*_Left) != typeid(*_Right))
            return false;
        return *_Left == *_Right;
    }
};

#include <boost/serialization/list.hpp>
void test_list(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    std::list<A *> alist;
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        A * free_a_ptr = new A;
        alist.push_back(free_a_ptr);
        alist.push_back(new A);
        // verify that first element is the same as the free pointer
        BOOST_CHECK((*alist.begin()) == free_a_ptr);
        oa << boost::serialization::make_nvp("alist", alist);
        oa << boost::serialization::make_nvp("free_a_ptr", free_a_ptr);
    }
    std::list<A *> alist1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        A * free_a_ptr1;
        ia >> boost::serialization::make_nvp("alist", alist1);
        ia >> boost::serialization::make_nvp("free_a_ptr", free_a_ptr1);
        BOOST_CHECK(
            alist.size() == alist1.size()
            && std::equal(alist.begin(),alist.end(),alist1.begin(),ptr_equal_to<A *>())
        );
        // verify that first element is the same as the free pointer
        BOOST_CHECK((*alist1.begin()) == free_a_ptr1);
    }

    std::for_each(
        alist.begin(),
        alist.end(),
        boost::checked_deleter<A>()
    );
    std::for_each(
        alist1.begin(),
        alist1.end(),
        boost::checked_deleter<A>()
    );
    std::remove(testfile);
}

int test_main( int /* argc */, char* /* argv */[] )
{
    test_list();
    return EXIT_SUCCESS;
}

// EOF
