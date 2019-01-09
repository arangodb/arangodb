//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : unit test for string comparison specializations
// *****************************************************************************

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/utils/is_cstring.hpp>

BOOST_AUTO_TEST_CASE( check_is_cstring_concept )
{
  namespace bp = boost::unit_test;
  BOOST_TEST((bp::is_cstring<char const*>::value));
  BOOST_TEST((bp::is_cstring<char const []>::value));
  BOOST_TEST((bp::is_cstring<char []>::value));
  BOOST_TEST((bp::is_cstring<char *>::value));
  BOOST_TEST((!bp::is_cstring<std::string>::value));
  BOOST_TEST((!bp::is_cstring< bp::basic_cstring<char> >::value));

  BOOST_TEST((!bp::is_cstring< std::vector<char> >::value));
}

BOOST_AUTO_TEST_CASE( check_is_cstring_comparable_concept )
{
  namespace bp = boost::unit_test;
  BOOST_TEST((bp::is_cstring_comparable<char const*>::value));
  BOOST_TEST((bp::is_cstring_comparable<char const []>::value));
  BOOST_TEST((bp::is_cstring_comparable<char []>::value));
  BOOST_TEST((bp::is_cstring_comparable<char *>::value));
  BOOST_TEST((bp::is_cstring_comparable<std::string>::value));
  BOOST_TEST((bp::is_cstring_comparable< bp::basic_cstring<char> >::value));

  BOOST_TEST((!bp::is_cstring_comparable< std::vector<char> >::value));
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( check_string_compare )
{
    char const* buf_ptr_cch     = "abc";
    char const  buf_array_cch[] = "abc";
    char        buf_array_ch[]  = "abc";
    char*       buf_ptr_ch      = buf_array_ch;
    std::string buf_str         = "abc";

    BOOST_TEST((void*)buf_ptr_cch != (void*)buf_array_cch);
    BOOST_TEST((void*)buf_ptr_cch != (void*)buf_array_ch);
    BOOST_TEST((void*)buf_array_cch != (void*)buf_array_ch);

    BOOST_TEST(buf_ptr_cch == buf_ptr_cch);
#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
    BOOST_TEST(buf_ptr_cch == buf_array_cch);
    BOOST_TEST(buf_ptr_cch == buf_array_ch);
    BOOST_TEST(buf_ptr_cch == buf_ptr_ch);
    BOOST_TEST(buf_ptr_cch == buf_str);
#endif

#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
    BOOST_TEST(buf_array_cch == buf_ptr_cch);
    BOOST_TEST(buf_array_cch == buf_array_cch);
    BOOST_TEST(buf_array_cch == buf_ptr_ch);
    BOOST_TEST(buf_array_cch == buf_array_ch);
    BOOST_TEST(buf_array_cch == buf_str);
#endif

    BOOST_TEST(buf_ptr_ch == buf_ptr_ch);
#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
    BOOST_TEST(buf_ptr_ch == buf_ptr_cch);
    BOOST_TEST(buf_ptr_ch == buf_array_cch);
    BOOST_TEST(buf_ptr_ch == buf_array_ch);
    BOOST_TEST(buf_ptr_ch == buf_str);
#endif

#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
    BOOST_TEST(buf_array_ch == buf_ptr_cch);
    BOOST_TEST(buf_array_ch == buf_array_cch);
    BOOST_TEST(buf_array_ch == buf_ptr_ch);
    BOOST_TEST(buf_array_ch == buf_array_ch);
    BOOST_TEST(buf_array_ch == buf_str);
#endif

    BOOST_TEST(buf_str == buf_ptr_cch);
    BOOST_TEST(buf_str == buf_array_cch);
    BOOST_TEST(buf_str == buf_ptr_ch);
    BOOST_TEST(buf_str == buf_array_ch);
    BOOST_TEST(buf_str == buf_str);

    BOOST_TEST(buf_ptr_cch == buf_str);
    //BOOST_TEST(buf_array_cch == buf_str); // does not compile
    BOOST_TEST(buf_ptr_ch == buf_str);
    //BOOST_TEST(buf_array_ch == buf_str); // does not compile
    BOOST_TEST(buf_str == buf_str);

#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
    BOOST_TEST( buf_ptr_cch == buf_ptr_cch, boost::test_tools::per_element() );
    BOOST_TEST( buf_ptr_cch <= "abd" , boost::test_tools::per_element() );
    BOOST_TEST( buf_ptr_cch >= "aba" , boost::test_tools::per_element() );
    BOOST_TEST( buf_str == buf_ptr_cch , boost::test_tools::per_element() );
    BOOST_TEST( buf_str <= "abd" , boost::test_tools::per_element() );
    BOOST_TEST( buf_str >= "aba" , boost::test_tools::per_element() );

    BOOST_TEST( buf_ptr_cch <= buf_ptr_cch, boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch >= buf_ptr_cch, boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch <= "abc" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch < "abd" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch < "abcd" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch >= "abc" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch > "aba" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_ptr_cch > "abad" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_str <= buf_ptr_cch , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_str >= buf_ptr_cch , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_str <= "abc" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_str < "abd" , boost::test_tools::lexicographic() );
    BOOST_TEST( buf_str > "aba" , boost::test_tools::lexicographic() );
#endif

}

// EOF
