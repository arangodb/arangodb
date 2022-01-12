//
// registered_buffer.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/registered_buffer.hpp>

#include "unit_test.hpp"

//------------------------------------------------------------------------------

// registered_buffer_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all the mutable_registered_buffer and
// const_registered_buffer classes compile and link correctly. Runtime
// failures are ignored.

namespace registered_buffer_compile {

using namespace boost::asio;

void test()
{
  try
  {
    // mutable_registered_buffer constructors.

    mutable_registered_buffer mb1;
    mutable_registered_buffer mb2(mb1);
    (void)mb2;

    // mutable_registered_buffer functions.

    mutable_buffer b1 = mb1.buffer();
    (void)b1;

    void* ptr1 = mb1.data();
    (void)ptr1;

    std::size_t n1 = mb1.size();
    (void)n1;

    registered_buffer_id id1 = mb1.id();
    (void)id1;

    // mutable_registered_buffer operators.

    mb1 += 128;
    mb1 = mb2 + 128;
    mb1 = 128 + mb2;

    // const_registered_buffer constructors.

    const_registered_buffer cb1;
    const_registered_buffer cb2(cb1);
    (void)cb2;
    const_registered_buffer cb3(mb1);
    (void)cb3;

    // const_registered_buffer functions.

    const_buffer b2 = cb1.buffer();
    (void)b2;

    const void* ptr2 = cb1.data();
    (void)ptr2;

    std::size_t n2 = cb1.size();
    (void)n2;

    registered_buffer_id id2 = cb1.id();
    (void)id2;

    // const_registered_buffer operators.

    cb1 += 128;
    cb1 = cb2 + 128;
    cb1 = 128 + cb2;

    // buffer function overloads.

    mb1 = buffer(mb2);
    mb1 = buffer(mb2, 128);
    cb1 = buffer(cb2);
    cb1 = buffer(cb2, 128);
  }
  catch (std::exception&)
  {
  }
}

} // namespace buffer_compile

//------------------------------------------------------------------------------

BOOST_ASIO_TEST_SUITE
(
  "registered_buffer",
  BOOST_ASIO_COMPILE_TEST_CASE(registered_buffer_compile::test)
)
