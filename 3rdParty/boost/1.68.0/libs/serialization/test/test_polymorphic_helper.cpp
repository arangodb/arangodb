/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_polymorphic.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com . 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef> // NULL
#include <cstdio> // remove
#include <fstream>

#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{ 
    using ::remove;
}
#endif

// the following is to ensure that when one of the libraries changes
// BJAM rebuilds and relinks the test.
/*
#include "polymorphic_text_archive.hpp"
#include "polymorphic_text_warchive.hpp"
#include "polymorphic_binary_archive.hpp"
#include "polymorphic_xml_archive.hpp"
#include "polymorphic_xml_warchive.hpp"
*/

#include <string>
#include <vector>

#include "test_tools.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/nvp.hpp>
// this test uses a special string (my_string) whose contents are shared
// and hence saved in the archive only once.  We need a helper in order
// to convert my_string into a serializable type

class my_string:public std::string
{
    typedef std::string super;

public:
    my_string(){}
    my_string(const super & str): super(str){}
    my_string & operator=(const super& rhs) {
      super::operator=(rhs);
      return *this;
    }
};

struct my_string_helper
{
  typedef std::vector<my_string> table;
  table m_t;
};

BOOST_SERIALIZATION_SPLIT_FREE(my_string)

namespace boost {
namespace serialization {

template<class Archive>
void save(Archive & ar, const my_string & str, const unsigned int /* version */)
{
    void (* const idx)(Archive &, const my_string &, const unsigned int) = & save;
    void * const id = reinterpret_cast<void * const>(idx);
    my_string_helper & msh = ar.template get_helper<my_string_helper>(id);

    my_string_helper::table t = msh.m_t;
    my_string_helper::table::iterator it = std::find(t.begin(), t.end(), str);
    if(it == t.end()){
        my_string_helper::table::size_type s = t.size();
        ar << make_nvp("index", s);
        t.push_back(str);
        ar << make_nvp("string", static_cast<const std::string &>(str));
    }
    else{
        my_string_helper::table::size_type s = it - t.begin();
        ar << make_nvp("index", s);
    }
}

template<class Archive>
void load(Archive & ar, my_string & str, const unsigned int /* version */)
{
    void (* const idx)(Archive &, my_string &, const unsigned int) = & load;
    void * const id = reinterpret_cast<void * const>(idx);
    my_string_helper & msh = ar.template get_helper<my_string_helper>(id);

    my_string_helper::table t = msh.m_t;

    my_string_helper::table::size_type s;
    ar >> make_nvp("index", s);
    t.reserve(s);
    if(s >= t.size()){
        std::string tmp;
        ar >> make_nvp("string", tmp);
        str = tmp;
        t.push_back(str);
    }
    else{
        str = t[s];
    }
}

} // namespace serialization
} // namespace boost
#include <boost/archive/polymorphic_oarchive.hpp>
#include <boost/archive/polymorphic_iarchive.hpp>

int test_main(int /* argc */, char * /* argv */ [])
{
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    std::vector<my_string> v1;
    for(int i=0; i<1000; ++i){
        v1.push_back(my_string(boost::lexical_cast<std::string>(i % 100)));
    }

    // test using using polymorphic implementation.
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa_implementation(os, TEST_ARCHIVE_FLAGS);
        oa_implementation << boost::serialization::make_nvp("vector", v1);
    }
    {
        std::vector<my_string> v2;
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive  ia_implementation(is, TEST_ARCHIVE_FLAGS);
        ia_implementation >> boost::serialization::make_nvp("vector", v2);
        BOOST_CHECK(v1 == v2);
    }
    std::remove(testfile);

    // test using using polymorphic interface.
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa_implementation(os, TEST_ARCHIVE_FLAGS);
        boost::archive::polymorphic_oarchive & oa_interface = oa_implementation;
        oa_interface << boost::serialization::make_nvp("vector", v1);
    }
    {
        std::vector<my_string> v2;
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive  ia_implementation(is, TEST_ARCHIVE_FLAGS);
        boost::archive::polymorphic_iarchive & ia_interface = ia_implementation;
        ia_interface >> boost::serialization::make_nvp("vector", v2);
        BOOST_CHECK(v1 == v2);
    }
    std::remove(testfile);
    std::remove(testfile);
    return EXIT_SUCCESS;
}
