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

#include "asio/detail/config.hpp"
#include <iostream>
#include "asio/detail/atomic_count.hpp"

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

#if defined(ASIO_MSVC)
# pragma warning (disable:4127)
# pragma warning (push)
# pragma warning (disable:4244)
# pragma warning (disable:4702)
#endif // defined(ASIO_MSVC)

#if !defined(ASIO_TEST_IOSTREAM)
# define ASIO_TEST_IOSTREAM std::cerr
#endif // !defined(ASIO_TEST_IOSTREAM)

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
  asio::detail::test_name();
  asio::detail::test_errors();
  ASIO_TEST_IOSTREAM << name << " test suite begins" << std::endl;
}

inline int end_test_suite(const char* name)
{
  ASIO_TEST_IOSTREAM << name << " test suite ends" << std::endl;
  ASIO_TEST_IOSTREAM << "\n*** ";
  long errors = asio::detail::test_errors();
  if (errors == 0)
    ASIO_TEST_IOSTREAM << "No errors detected.";
  else if (errors == 1)
    ASIO_TEST_IOSTREAM << "1 error detected.";
  else
    ASIO_TEST_IOSTREAM << errors << " errors detected." << std::endl;
  ASIO_TEST_IOSTREAM << std::endl;
  return errors == 0 ? 0 : 1;
}

template <void (*Test)()>
inline void run_test(const char* name)
{
  test_name() = name;
  long errors_before = asio::detail::test_errors();
  Test();
  if (test_errors() == errors_before)
    ASIO_TEST_IOSTREAM << name << " passed" << std::endl;
  else
    ASIO_TEST_IOSTREAM << name << " failed" << std::endl;
}

template <void (*)()>
inline void compile_test(const char* name)
{
  ASIO_TEST_IOSTREAM << name << " passed" << std::endl;
}

#if defined(ASIO_NO_EXCEPTIONS)

template <typename T>
void throw_exception(const T& t)
{
  ASIO_TEST_IOSTREAM << "Exception: " << t.what() << std::endl;
  std::abort();
}

#endif // defined(ASIO_NO_EXCEPTIONS)

} // namespace detail
} // namespace asio

#define ASIO_CHECK(expr) \
  do { if (!(expr)) { \
    ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << asio::detail::test_name() << ": " \
      << "check '" << #expr << "' failed" << std::endl; \
    ++asio::detail::test_errors(); \
  } } while (0)

#define ASIO_CHECK_MESSAGE(expr, msg) \
  do { if (!(expr)) { \
    ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << asio::detail::test_name() << ": " \
      << msg << std::endl; \
    ++asio::detail::test_errors(); \
  } } while (0)

#define ASIO_WARN_MESSAGE(expr, msg) \
  do { if (!(expr)) { \
    ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << asio::detail::test_name() << ": " \
      << msg << std::endl; \
  } } while (0)

#define ASIO_ERROR(msg) \
  do { \
    ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
      << asio::detail::test_name() << ": " \
      << msg << std::endl; \
    ++asio::detail::test_errors(); \
  } while (0)

#define ASIO_TEST_SUITE(name, tests) \
  int main() \
  { \
    asio::detail::begin_test_suite(name); \
    tests \
    return asio::detail::end_test_suite(name); \
  }

#define ASIO_TEST_CASE(test) \
  asio::detail::run_test<&test>(#test);

#define ASIO_COMPILE_TEST_CASE(test) \
  asio::detail::compile_test<&test>(#test);

inline void null_test()
{
}

#if defined(__GNUC__) && defined(_AIX)

// AIX needs this symbol defined in asio, even if it doesn't do anything.
int test_main(int, char**)
{
}

#endif // defined(__GNUC__) && defined(_AIX)

#if defined(ASIO_MSVC)
# pragma warning (pop)
#endif // defined(ASIO_MSVC)

#endif // UNIT_TEST_HPP
