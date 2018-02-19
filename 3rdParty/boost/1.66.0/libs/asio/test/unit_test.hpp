//
// unit_test.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef UNIT_TEST_HPP
#define UNIT_TEST_HPP

#include <boost/asio/detail/config.hpp>
#include <iostream>
#include <boost/asio/detail/atomic_count.hpp>

#if defined(__sun)
# include <stdlib.h> // Needed for lrand48.
#endif // defined(__sun)

#if defined(__BORLANDC__)

// Prevent use of intrinsic for strcmp.
# include <cstring>
# undef strcmp
 
// Suppress error about condition always being true.
# pragma option -w-ccc

#endif // defined(__BORLANDC__)

#if defined(BOOST_ASIO_MSVC)
# pragma warning (disable:4127)
# pragma warning (push)
# pragma warning (disable:4244)
# pragma warning (disable:4702)
#endif // defined(BOOST_ASIO_MSVC)

#if !defined(BOOST_ASIO_TEST_IOSTREAM)
# define BOOST_ASIO_TEST_IOSTREAM std::cerr
#endif // !defined(BOOST_ASIO_TEST_IOSTREAM)

namespace boost {
namespace asio {
namespace detail {

inline const char*& test_name()
{
  static const char* name = 0;
  return name;
}

inline atomic_count& test_errors()
{
  static atomic_count errors(0);
  return errors;
}

inline void begin_test_suite(const char* name)
{
  boost::asio::detail::test_name();
  boost::asio::detail::test_errors();
  BOOST_ASIO_TEST_IOSTREAM << name << " test suite begins" << std::endl;
}

inline int end_test_suite(const char* name)
{
  BOOST_ASIO_TEST_IOSTREAM << name << " test suite ends" << std::endl;
  BOOST_ASIO_TEST_IOSTREAM << "\n*** ";
  long errors = boost::asio::detail::test_errors();
  if (errors == 0)
    BOOST_ASIO_TEST_IOSTREAM << "No errors detected.";
  else if (errors == 1)
    BOOST_ASIO_TEST_IOSTREAM << "1 error detected.";
  else
    BOOST_ASIO_TEST_IOSTREAM << errors << " errors detected." << std::endl;
  BOOST_ASIO_TEST_IOSTREAM << std::endl;
  return errors == 0 ? 0 : 1;
}

template <void (*Test)()>
inline void run_test(const char* name)
{
  test_name() = name;
  long errors_before = boost::asio::detail::test_errors();
  Test();
  if (test_errors() == errors_before)
    BOOST_ASIO_TEST_IOSTREAM << name << " passed" << std::endl;
  else
    BOOST_ASIO_TEST_IOSTREAM << name << " failed" << std::endl;
}

template <void (*)()>
inline void compile_test(const char* name)
{
  BOOST_ASIO_TEST_IOSTREAM << name << " passed" << std::endl;
}

#if defined(BOOST_ASIO_NO_EXCEPTIONS)

template <typename T>
void throw_exception(const T& t)
{
  BOOST_ASIO_TEST_IOSTREAM << "Exception: " << t.what() << std::endl;
  std::abort();
}

#endif // defined(BOOST_ASIO_NO_EXCEPTIONS)

} // namespace detail
} // namespace asio
} // namespace boost

#define BOOST_ASIO_CHECK(expr) \
  do { if (!(expr)) { \
    BOOST_ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << boost::asio::detail::test_name() << ": " \
      << "check '" << #expr << "' failed" << std::endl; \
    ++boost::asio::detail::test_errors(); \
  } } while (0)

#define BOOST_ASIO_CHECK_MESSAGE(expr, msg) \
  do { if (!(expr)) { \
    BOOST_ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << boost::asio::detail::test_name() << ": " \
      << msg << std::endl; \
    ++boost::asio::detail::test_errors(); \
  } } while (0)

#define BOOST_ASIO_WARN_MESSAGE(expr, msg) \
  do { if (!(expr)) { \
    BOOST_ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << boost::asio::detail::test_name() << ": " \
      << msg << std::endl; \
  } } while (0)

#define BOOST_ASIO_ERROR(msg) \
  do { \
    BOOST_ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << boost::asio::detail::test_name() << ": " \
      << msg << std::endl; \
    ++boost::asio::detail::test_errors(); \
  } while (0)

#define BOOST_ASIO_TEST_SUITE(name, tests) \
  int main() \
  { \
    boost::asio::detail::begin_test_suite(name); \
    tests \
    return boost::asio::detail::end_test_suite(name); \
  }

#define BOOST_ASIO_TEST_CASE(test) \
  boost::asio::detail::run_test<&test>(#test);

#define BOOST_ASIO_COMPILE_TEST_CASE(test) \
  boost::asio::detail::compile_test<&test>(#test);

inline void null_test()
{
}

#if defined(__GNUC__) && defined(_AIX)

// AIX needs this symbol defined in asio, even if it doesn't do anything.
int test_main(int, char**)
{
}

#endif // defined(__GNUC__) && defined(_AIX)

#if defined(BOOST_ASIO_MSVC)
# pragma warning (pop)
#endif // defined(BOOST_ASIO_MSVC)

#endif // UNIT_TEST_HPP
