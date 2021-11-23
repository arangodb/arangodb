//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/test for the library home page.
//

//[snippet12
#define __BOOST_TEST_MODULE__ MyTest
#include <boost/test/unit_test.hpp>

int add( int i, int j ) { return i + j; }

__BOOST_AUTO_TEST_CASE__(my_test)
{
  // six ways to detect and report the same error:

  // continues on error
  __BOOST_TEST__( add(2, 2) == 4 );          /*<
                                          This approach uses tool __BOOST_TEST__, which displays an error message (by default on `std::cout`) that includes
                                          the expression that failed, as well as the values on the two side of the equation, the source file name,
                                          and the source file line number. It also increments the error count. At program termination,
                                          the error count will be displayed automatically by the __UTF__.>*/
  
  // throws on error
  __BOOST_TEST_REQUIRE__( add(2, 2) == 4 );  /*<
                                          This approach uses tool __BOOST_TEST_REQUIRE__, is similar to approach #1, except that after displaying the error,
                                          an exception is thrown, to be caught by the __UTF__. This approach is suitable when writing an
                                          explicit test program, and the error would be so severe as to make further testing impractical.
                                          >*/
  
  //continues on error
  if (add(2, 2) != 4)
    __BOOST_ERROR__( "Ouch..." );            /*< 
                                          This approach is similar to approach #1, except that the error detection and error reporting are coded separately.
                                          This is most useful when the specific condition being tested requires several independent statements and/or is
                                          not indicative of the reason for failure.
                                          >*/
  
  // throws on error
  if (add(2, 2) != 4)
    __BOOST_FAIL__( "Ouch..." );             /*<
                                         This approach is similar to approach #2, except that the error detection and error reporting are coded separately.
                                         This is most useful when the specific condition being tested requires several independent statements and/or is
                                         not indicative of the reason for failure.
                                         >*/
  
  // throws on error
  if (add(2, 2) != 4)
    throw "Ouch...";                     /*<
                                         This approach throws an exception, which will be caught and reported by the __UTF__. The error
                                         message displayed when the exception is caught will be most meaningful if the exception is derived from
                                         `std::exception`, or is a `char*` or `std::string`.
                                         >*/
  
  // continues on error
  __BOOST_TEST__( add(2, 2) == 4,            /*<
                                                           This approach uses tool __BOOST_TEST__ with additional message argument, is similar to approach #1, 
                                                           except that similar to the approach #3 displays an alternative error message specified as a second argument.
                                                           >*/
              "2 plus 2 is not 4 but " << add(2, 2));
}
//]
