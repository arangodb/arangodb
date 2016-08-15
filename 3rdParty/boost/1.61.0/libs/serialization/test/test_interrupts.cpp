/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_array.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com . 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <stdlib.h>

#include <boost/config.hpp>
#include <cstddef>
#include <fstream>
#include <algorithm> // equal
#include <cstdio> // remove
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{ 
    using ::remove;
}
#endif

#include "test_tools.hpp"
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/archive/archive_exception.hpp>

struct test_dummy_out {
    template<class Archive>
    void save(Archive & ar, const unsigned int /*version*/) const {
        throw boost::archive::archive_exception(
            boost::archive::archive_exception::other_exception
        );
    }
    template<class Archive>
    void load(Archive & ar, const unsigned int version){
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
    test_dummy_out(){}
};

template<class T>
int test_out(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    const T t;
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        {
            BOOST_TRY {
                test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
                bool exception_invoked = false;
                BOOST_TRY {
                    oa << BOOST_SERIALIZATION_NVP(t);
                }
                BOOST_CATCH (boost::archive::archive_exception ae){
                    BOOST_CHECK(
                        boost::archive::archive_exception::other_exception
                        == ae.code
                    );
                    exception_invoked = true;
                }
                BOOST_CATCH_END
                BOOST_CHECK(exception_invoked);
            }
            BOOST_CATCH (boost::archive::archive_exception ae){}
            BOOST_CATCH_END
        }
        os.close();
    }
    std::remove(testfile);
    return EXIT_SUCCESS;
}

struct test_dummy_in {
    template<class Archive>
    void save(Archive & ar, const unsigned int /*version*/) const {
    }
    template<class Archive>
    void load(Archive & ar, const unsigned int /*version*/){
        throw boost::archive::archive_exception(
            boost::archive::archive_exception::other_exception
        );
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
    test_dummy_in(){}
};

template<class T>
int test_in(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    const T t;
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        {
            test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
            oa << BOOST_SERIALIZATION_NVP(t);
        }
        os.close();
    }
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        {
            T t1;
            BOOST_TRY {
                test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
                bool exception_invoked = false;
                BOOST_TRY {
                    ia >> BOOST_SERIALIZATION_NVP(t1);
                }
                BOOST_CATCH (boost::archive::archive_exception ae){
                    BOOST_CHECK(
                        boost::archive::archive_exception::other_exception
                        == ae.code
                    );
                    exception_invoked = true;
                }
                BOOST_CATCH_END
                BOOST_CHECK(exception_invoked);
            }
            BOOST_CATCH (boost::archive::archive_exception ae){}
            BOOST_CATCH_END
        }
        is.close();
    }
    std::remove(testfile);
    return EXIT_SUCCESS;
}

int test_main( int /* argc */, char* /* argv */[] )
{
    int res;
    res = test_out<test_dummy_out>();
    if (res != EXIT_SUCCESS)
        return EXIT_FAILURE;
    res = test_in<test_dummy_in>();
    if (res != EXIT_SUCCESS)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

// EOF
