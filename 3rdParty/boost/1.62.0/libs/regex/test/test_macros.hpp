///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifndef BOOST_MULTIPRECISION_TEST_HPP
#define BOOST_MULTIPRECISION_TEST_HPP

#include <limits>
#include <cmath>
#include <typeinfo>
#include <iostream>
#include <iomanip>
#include <stdlib.h>

#include <boost/core/lightweight_test.hpp>
#include <boost/current_function.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility/enable_if.hpp>

enum
{
   warn_on_fail,
   error_on_fail,
   abort_on_fail
};

template <class T>
inline int digits_of(const T&)
{
   return std::numeric_limits<T>::is_specialized ? std::numeric_limits<T>::digits : 18;
}


inline std::ostream& report_where(const char* file, int line, const char* function)
{
   if(function)
      BOOST_LIGHTWEIGHT_TEST_OSTREAM << "In function: "<< function << std::endl;
   BOOST_LIGHTWEIGHT_TEST_OSTREAM << file << ":" << line;
   return BOOST_LIGHTWEIGHT_TEST_OSTREAM;
}

#define BOOST_MP_REPORT_WHERE report_where(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION)

inline void report_severity(int severity)
{
   if(severity == error_on_fail)
      ++boost::detail::test_errors();
   else if(severity == abort_on_fail)
   {
      ++boost::detail::test_errors();
      abort();
   }
}

#define BOOST_MP_REPORT_SEVERITY(severity) report_severity(severity)

template <class E>
void report_unexpected_exception(const E& e, int severity, const char* file, int line, const char* function)
{
   report_where(file, line, function)  << " Unexpected exception of type " << typeid(e).name() << std::endl;
   BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Errot message was: " << e.what() << std::endl;
   BOOST_MP_REPORT_SEVERITY(severity);
}

#define BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity) \
   catch(const std::exception& e) \
   {  report_unexpected_exception(e, severity, __FILE__, __LINE__, BOOST_CURRENT_FUNCTION); }\
   catch(...)\
   {  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Exception of unknown type was thrown" << std::endl; report_severity(severity); }


#define BOOST_CHECK_IMP(x, severity)\
   try{ if(x){}else{\
   BOOST_MP_REPORT_WHERE << " Failed predicate: " << BOOST_STRINGIZE(x) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_CHECK(x) BOOST_CHECK_IMP(x, error_on_fail)
#define BOOST_WARN(x)  BOOST_CHECK_IMP(x, warn_on_fail)
#define BOOST_REQUIRE(x)  BOOST_CHECK_IMP(x, abort_on_fail)

#define BOOST_EQUAL_IMP(x, y, severity)\
   try{ if(!((x) == (y))){\
   BOOST_MP_REPORT_WHERE << " Failed check for equality: \n" \
   << std::setprecision(digits_of(x)) << std::scientific\
   << "Value of LHS was: " << (x) << "\n"\
   << "Value of RHS was: " << (y) << "\n"\
   << std::setprecision(3) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_NE_IMP(x, y, severity)\
   try{ if(!(x != y)){\
   BOOST_MP_REPORT_WHERE << " Failed check for non-equality: \n" \
   << std::setprecision(digits_of(x)) << std::scientific\
   << "Value of LHS was: " << x << "\n"\
   << "Value of RHS was: " << y << "\n"\
   << std::setprecision(3) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_LT_IMP(x, y, severity)\
   try{ if(!(x < y)){\
   BOOST_MP_REPORT_WHERE << " Failed check for less than: \n" \
   << std::setprecision(digits_of(x)) << std::scientific\
   << "Value of LHS was: " << x << "\n"\
   << "Value of RHS was: " << y << "\n"\
   << std::setprecision(3) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_GT_IMP(x, y, severity)\
   try{ if(!(x > y)){\
   BOOST_MP_REPORT_WHERE << " Failed check for greater than: \n" \
   << std::setprecision(digits_of(x)) << std::scientific\
   << "Value of LHS was: " << x << "\n"\
   << "Value of RHS was: " << y << "\n"\
   << std::setprecision(3) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_LE_IMP(x, y, severity)\
   try{ if(!(x <= y)){\
   BOOST_MP_REPORT_WHERE << " Failed check for less-than-equal-to: \n" \
   << std::setprecision(digits_of(x)) << std::scientific\
   << "Value of LHS was: " << x << "\n"\
   << "Value of RHS was: " << y << "\n"\
   << std::setprecision(3) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_GE_IMP(x, y, severity)\
   try{ if(!(x >= y)){\
   BOOST_MP_REPORT_WHERE << " Failed check for greater-than-equal-to \n" \
   << std::setprecision(digits_of(x)) << std::scientific\
   << "Value of LHS was: " << x << "\n"\
   << "Value of RHS was: " << y << "\n"\
   << std::setprecision(3) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   }BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

#define BOOST_MT_CHECK_THROW_IMP(x, E, severity)\
   try{ \
      x;\
   BOOST_MP_REPORT_WHERE << " Expected exception not thrown in expression " << BOOST_STRINGIZE(x) << std::endl;\
   BOOST_MP_REPORT_SEVERITY(severity);\
   }\
   catch(const E&){}\
   BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)

template <class I, class J>
bool check_equal_collections(I a, I b, J x, J y)
{
   int i = 0;
   while(a != b)
   {
      if(x == y)
      {
         BOOST_LIGHTWEIGHT_TEST_OSTREAM << " Unexpected end of second sequence" << std::endl;
         return false;
      }
      if(*a != *x)
      {
         BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Error occured in position " << i << " of the collection." << std::endl;
         BOOST_LIGHTWEIGHT_TEST_OSTREAM << "First value was " << std::setprecision(digits_of(x)) << std::scientific << *a << std::endl;
         BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Second value was " << std::setprecision(digits_of(x)) << std::scientific << *x << std::endl;
         return false;
      }
      ++a;
      ++x;
   }
   return true;
}

#define BOOST_MT_CHECK_EQ_COLLECTIONS(a, b, x, y, severity)\
   try{ \
      if(!check_equal_collections(a, b, x, y))\
      {\
         BOOST_MP_REPORT_WHERE << " Collections were not equal" << std::endl;\
         BOOST_MP_REPORT_SEVERITY(severity);\
      }\
   }\
   BOOST_MP_UNEXPECTED_EXCEPTION_CHECK(severity)


#define BOOST_CHECK_EQUAL(x, y) BOOST_EQUAL_IMP(x, y, error_on_fail)
#define BOOST_WARN_EQUAL(x, y) BOOST_EQUAL_IMP(x, y, warn_on_fail)
#define BOOST_REQUIRE_EQUAL(x, y) BOOST_EQUAL_IMP(x, y, abort_on_fail)

#define BOOST_CHECK_NE(x, y) BOOST_NE_IMP(x, y, error_on_fail)
#define BOOST_WARN_NE(x, y) BOOST_NE_IMP(x, y, warn_on_fail)
#define BOOST_REQUIRE_NE(x, y) BOOST_NE_IMP(x, y, abort_on_fail)

#define BOOST_CHECK_LT(x, y) BOOST_LT_IMP(x, y, error_on_fail)
#define BOOST_WARN_LT(x, y) BOOST_LT_IMP(x, y, warn_on_fail)
#define BOOST_REQUIRE_LT(x, y) BOOST_LT_IMP(x, y, abort_on_fail)

#define BOOST_CHECK_GT(x, y) BOOST_GT_IMP(x, y, error_on_fail)
#define BOOST_WARN_GT(x, y) BOOST_GT_IMP(x, y, warn_on_fail)
#define BOOST_REQUIRE_GT(x, y) BOOST_GT_IMP(x, y, abort_on_fail)

#define BOOST_CHECK_LE(x, y) BOOST_LE_IMP(x, y, error_on_fail)
#define BOOST_WARN_LE(x, y) BOOST_LE_IMP(x, y, warn_on_fail)
#define BOOST_REQUIRE_LE(x, y) BOOST_LE_IMP(x, y, abort_on_fail)

#define BOOST_CHECK_GE(x, y) BOOST_GE_IMP(x, y, error_on_fail)
#define BOOST_WARN_GE(x, y) BOOST_GE_IMP(x, y, warn_on_fail)
#define BOOST_REQUIRE_GE(x, y) BOOST_GE_IMP(x, y, abort_on_fail)

#define BOOST_CHECK_THROW(x, E) BOOST_MT_CHECK_THROW_IMP(x, E, error_on_fail)
#define BOOST_WARN_THROW(x, E) BOOST_MT_CHECK_THROW_IMP(x, E, warn_on_fail)
#define BOOST_REQUIRE_THROW(x, E) BOOST_MT_CHECK_THROW_IMP(x, E, abort_on_fail)

#define BOOST_CHECK_EQUAL_COLLECTIONS(a, b, x, y) BOOST_MT_CHECK_EQ_COLLECTIONS(a, b, x, y, error_on_fail)
#define BOOST_WARN_EQUAL_COLLECTIONS(a, b, x, y) BOOST_MT_CHECK_EQ_COLLECTIONS(a, b, x, y, warn_on_fail)
#define BOOST_REQUIRE_EQUAL_COLLECTIONS(a, b, x, y) BOOST_MT_CHECK_EQ_COLLECTIONS(a, b, x, y, abort_on_fail)

#endif
