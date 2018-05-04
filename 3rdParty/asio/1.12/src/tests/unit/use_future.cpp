//
// use_future.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/use_future.hpp"

#include <string>
#include "unit_test.hpp"

#if defined(ASIO_HAS_STD_FUTURE)

#include "archetypes/async_ops.hpp"
#include "archetypes/deprecated_async_ops.hpp"

void use_future_0_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<void> f;

  f = async_op_0(use_future);
  try
  {
    f.get();
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_0(true, use_future);
  try
  {
    f.get();
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_0(false, use_future);
  try
  {
    f.get();
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_0(true, use_future);
  try
  {
    f.get();
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_0(false, use_future);
  try
  {
    f.get();
    ASIO_CHECK(false);
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

void use_future_1_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;

  f = async_op_1(use_future);
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_1(true, use_future);
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_1(false, use_future);
  try
  {
    int i = f.get();
    ASIO_CHECK(false);
    (void)i;
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_1(true, use_future);
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_1(false, use_future);
  try
  {
    int i = f.get();
    ASIO_CHECK(false);
    (void)i;
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

void use_future_2_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<std::tuple<int, double>> f;

  f = async_op_2(use_future);
  try
  {
    int i;
    double d;
    std::tie(i, d) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_2(true, use_future);
  try
  {
    int i;
    double d;
    std::tie(i, d) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_2(false, use_future);
  try
  {
    std::tuple<int, double> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_2(true, use_future);
  try
  {
    int i;
    double d;
    std::tie(i, d) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_2(false, use_future);
  try
  {
    std::tuple<int, double> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

void use_future_3_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<std::tuple<int, double, char>> f;

  f = async_op_3(use_future);
  try
  {
    int i;
    double d;
    char c;
    std::tie(i, d, c) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
    ASIO_CHECK(c == 'a');
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_3(true, use_future);
  try
  {
    int i;
    double d;
    char c;
    std::tie(i, d, c) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
    ASIO_CHECK(c == 'a');
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_3(false, use_future);
  try
  {
    std::tuple<int, double, char> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_3(true, use_future);
  try
  {
    int i;
    double d;
    char c;
    std::tie(i, d, c) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
    ASIO_CHECK(c == 'a');
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_3(false, use_future);
  try
  {
    std::tuple<int, double, char> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

int package_0()
{
  return 42;
}

int package_ec_0(asio::error_code ec)
{
  return ec ? 0 : 42;
}

int package_ex_0(std::exception_ptr ex)
{
  return ex ? 0 : 42;
}

void use_future_package_0_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;

  f = async_op_0(use_future(package_0));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_0(true, use_future(&package_ec_0));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_0(false, use_future(package_ec_0));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_0(true, use_future(package_ex_0));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_0(false, use_future(package_ex_0));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

int package_1(int i)
{
  return i;
}

int package_ec_1(asio::error_code ec, int i)
{
  return ec ? 0 : i;
}

int package_ex_1(std::exception_ptr ex, int i)
{
  return ex ? 0 : i;
}

void use_future_package_1_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;

  f = async_op_1(use_future(package_1));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_1(true, use_future(package_ec_1));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_1(false, use_future(package_ec_1));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_1(true, use_future(package_ex_1));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_1(false, use_future(package_ex_1));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

int package_2(int i, double)
{
  return i;
}

int package_ec_2(asio::error_code ec, int i, double)
{
  return ec ? 0 : i;
}

int package_ex_2(std::exception_ptr ex, int i, double)
{
  return ex ? 0 : i;
}

void use_future_package_2_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;

  f = async_op_2(use_future(package_2));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_2(true, use_future(package_ec_2));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_2(false, use_future(package_ec_2));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_2(true, use_future(package_ex_2));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_2(false, use_future(package_ex_2));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

int package_3(int i, double, char)
{
  return i;
}

int package_ec_3(asio::error_code ec, int i, double, char)
{
  return ec ? 0 : i;
}

int package_ex_3(std::exception_ptr ex, int i, double, char)
{
  return ex ? 0 : i;
}

void use_future_package_3_test()
{
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;

  f = async_op_3(use_future(package_3));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_3(true, use_future(package_ec_3));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ec_3(false, use_future(package_ec_3));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_3(true, use_future(package_ex_3));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_3(false, use_future(package_ex_3));
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
}

void deprecated_use_future_0_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<void> f;
  asio::io_context ctx;

  f = deprecated_async_op_0(ctx, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    f.get();
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_0(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    f.get();
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_0(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    f.get();
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_0(true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    f.get();
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = async_op_ex_0(false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    f.get();
    ASIO_CHECK(false);
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_1_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;
  asio::io_context ctx;

  f = deprecated_async_op_1(ctx, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_1(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_1(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(false);
    (void)i;
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_1(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_1(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(false);
    (void)i;
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_2_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<std::tuple<int, double>> f;
  asio::io_context ctx;

  f = deprecated_async_op_2(ctx, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i;
    double d;
    std::tie(i, d) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_2(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i;
    double d;
    std::tie(i, d) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_2(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    std::tuple<int, double> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_2(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i;
    double d;
    std::tie(i, d) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_2(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    std::tuple<int, double> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_3_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<std::tuple<int, double, char>> f;
  asio::io_context ctx;

  f = deprecated_async_op_3(ctx, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i;
    double d;
    char c;
    std::tie(i, d, c) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
    ASIO_CHECK(c == 'a');
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_3(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i;
    double d;
    char c;
    std::tie(i, d, c) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
    ASIO_CHECK(c == 'a');
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_3(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    std::tuple<int, double, char> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::operation_aborted);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_3(ctx, true, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    int i;
    double d;
    char c;
    std::tie(i, d, c) = f.get();
    ASIO_CHECK(i == 42);
    ASIO_CHECK(d == 2.0);
    ASIO_CHECK(c == 'a');
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_3(ctx, false, use_future);
  ctx.restart();
  ctx.run();
  try
  {
    std::tuple<int, double, char> t = f.get();
    ASIO_CHECK(false);
    (void)t;
  }
  catch (std::exception& e)
  {
    ASIO_CHECK(e.what() == std::string("blah"));
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_package_0_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;
  asio::io_context ctx;

  f = deprecated_async_op_0(ctx, use_future(package_0));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_0(ctx, true, use_future(&package_ec_0));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_0(ctx, false, use_future(package_ec_0));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_0(ctx, true, use_future(package_ex_0));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_0(ctx, false, use_future(package_ex_0));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_package_1_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;
  asio::io_context ctx;

  f = deprecated_async_op_1(ctx, use_future(package_1));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_1(ctx, true, use_future(package_ec_1));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_1(ctx, false, use_future(package_ec_1));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_1(ctx, true, use_future(package_ex_1));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_1(ctx, false, use_future(package_ex_1));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_package_2_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;
  asio::io_context ctx;

  f = deprecated_async_op_2(ctx, use_future(package_2));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_2(ctx, true, use_future(package_ec_2));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_2(ctx, false, use_future(package_ec_2));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_2(ctx, true, use_future(package_ex_2));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_2(ctx, false, use_future(package_ex_2));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

void deprecated_use_future_package_3_test()
{
#if !defined(ASIO_NO_DEPRECATED)
  using asio::use_future;
  using namespace archetypes;

  std::future<int> f;
  asio::io_context ctx;

  f = deprecated_async_op_3(ctx, use_future(package_3));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_3(ctx, true, use_future(package_ec_3));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ec_3(ctx, false, use_future(package_ec_3));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_3(ctx, true, use_future(package_ex_3));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 42);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }

  f = deprecated_async_op_ex_3(ctx, false, use_future(package_ex_3));
  ctx.restart();
  ctx.run();
  try
  {
    int i = f.get();
    ASIO_CHECK(i == 0);
  }
  catch (...)
  {
    ASIO_CHECK(false);
  }
#endif // !defined(ASIO_NO_DEPRECATED)
}

ASIO_TEST_SUITE
(
  "use_future",
  ASIO_TEST_CASE(use_future_0_test)
  ASIO_TEST_CASE(use_future_1_test)
  ASIO_TEST_CASE(use_future_2_test)
  ASIO_TEST_CASE(use_future_3_test)
  ASIO_TEST_CASE(use_future_package_0_test)
  ASIO_TEST_CASE(use_future_package_1_test)
  ASIO_TEST_CASE(use_future_package_2_test)
  ASIO_TEST_CASE(use_future_package_3_test)
  ASIO_TEST_CASE(deprecated_use_future_0_test)
  ASIO_TEST_CASE(deprecated_use_future_1_test)
  ASIO_TEST_CASE(deprecated_use_future_2_test)
  ASIO_TEST_CASE(deprecated_use_future_3_test)
  ASIO_TEST_CASE(deprecated_use_future_package_0_test)
  ASIO_TEST_CASE(deprecated_use_future_package_1_test)
  ASIO_TEST_CASE(deprecated_use_future_package_2_test)
  ASIO_TEST_CASE(deprecated_use_future_package_3_test)
)

#else // defined(ASIO_HAS_STD_FUTURE)

ASIO_TEST_SUITE
(
  "use_future",
  ASIO_TEST_CASE(null_test)
)

#endif // defined(ASIO_HAS_STD_FUTURE)
