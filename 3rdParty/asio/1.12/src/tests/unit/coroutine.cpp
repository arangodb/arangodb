//
// coroutine.cpp
// ~~~~~~~~~~~~~
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
#include "asio/coroutine.hpp"

#include "unit_test.hpp"

// Must come after all other headers.
#include "asio/yield.hpp"

//------------------------------------------------------------------------------

// Coroutine completes via yield break.

void yield_break_coro(asio::coroutine& coro)
{
  reenter (coro)
  {
    yield return;
    yield break;
  }
}

void yield_break_test()
{
  asio::coroutine coro;
  ASIO_CHECK(!coro.is_complete());
  yield_break_coro(coro);
  ASIO_CHECK(!coro.is_complete());
  yield_break_coro(coro);
  ASIO_CHECK(coro.is_complete());
}

//------------------------------------------------------------------------------

// Coroutine completes via return.

void return_coro(asio::coroutine& coro)
{
  reenter (coro)
  {
    return;
  }
}

void return_test()
{
  asio::coroutine coro;
  return_coro(coro);
  ASIO_CHECK(coro.is_complete());
}

//------------------------------------------------------------------------------

// Coroutine completes via exception.

void exception_coro(asio::coroutine& coro)
{
  reenter (coro)
  {
    throw 1;
  }
}

void exception_test()
{
  asio::coroutine coro;
  try { exception_coro(coro); } catch (int) {}
  ASIO_CHECK(coro.is_complete());
}

//------------------------------------------------------------------------------

// Coroutine completes by falling off the end.

void fall_off_end_coro(asio::coroutine& coro)
{
  reenter (coro)
  {
  }
}

void fall_off_end_test()
{
  asio::coroutine coro;
  fall_off_end_coro(coro);
  ASIO_CHECK(coro.is_complete());
}

//------------------------------------------------------------------------------

ASIO_TEST_SUITE
(
  "coroutine",
  ASIO_TEST_CASE(yield_break_test)
  ASIO_TEST_CASE(return_test)
  ASIO_TEST_CASE(exception_test)
  ASIO_TEST_CASE(fall_off_end_test)
)
