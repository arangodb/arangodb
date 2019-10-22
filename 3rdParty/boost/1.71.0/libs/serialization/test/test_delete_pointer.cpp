/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_delete_pointer.cpp

// (C) Copyright 2002 Vahan Margaryan. 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstddef> // NULL
#include <fstream>

#include <cstdio> // remove
#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{ 
    using ::remove;
}
#endif

#include "test_tools.hpp"
#include <boost/core/no_exceptions_support.hpp>
#include <boost/serialization/throw_exception.hpp>

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_member.hpp>

//A holds a pointer to another A, but doesn't own the pointer.
//objCount
class A
{
    friend class boost::serialization::access;
    template<class Archive>
    void save(Archive &ar, const unsigned int /* file_version */) const
    {
        ar << BOOST_SERIALIZATION_NVP(next_);
    }
    template<class Archive>
    void load(Archive & ar, const unsigned int /* file_version */)
    {
        ar >> BOOST_SERIALIZATION_NVP(next_);
        ++loadcount;
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
public:
    A()
    {
        if(test && objcount == 3)
            boost::serialization::throw_exception(boost::archive::archive_exception(
                boost::archive::archive_exception::no_exception
            ));
        next_ = 0;
        ++objcount;
    }
    ~A(){
        delete next_;
        --objcount;
    }
    A* next_;
    static int objcount;
    static bool test;
    static int loadcount;
};


int A::objcount = 0;
int A::loadcount = 0;
bool A::test = false;

int
test_main( int /* argc */, char* /* argv */[] )
{

    //fill the vector with chained A's. The vector is assumed
    //to own the objects - we will destroy the objects through this vector.

    A * head = new A;
    A* last = head;
    unsigned int i;
    for(i = 1; i < 9; ++i)
    {
        A *a = new A;
        last->next_ = a;
        last = a;
    }

    const char * testfile = boost::archive::tmpnam(0);
    BOOST_REQUIRE(NULL != testfile);

    //output the list
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << BOOST_SERIALIZATION_NVP(head);
    }

    delete head;
    BOOST_CHECK(A::objcount == 0);

    head = NULL;
    A::test = true;
    //read the list back
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        BOOST_TRY {
            ia >> BOOST_SERIALIZATION_NVP(head);
        }
        BOOST_CATCH (...){
            ia.delete_created_pointers();
        }
        BOOST_CATCH_END
    }

    //identify the leaks
    BOOST_CHECK(A::loadcount == 0);
    std::remove(testfile);
    return EXIT_SUCCESS;
}
