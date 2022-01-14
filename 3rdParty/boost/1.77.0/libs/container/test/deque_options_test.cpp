//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/deque.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::container;

void test_block_bytes()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = deque_options_t< block_bytes<128u> >;
   #else
   typedef deque_options< block_bytes<128u> >::type options_t;
   #endif
   typedef deque<unsigned short, void, options_t> deque_t;
   BOOST_TEST(deque_t::get_block_size() == 128u/sizeof(unsigned short));
}

void test_block_elements()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = deque_options_t< block_size<64> >;
   #else
   typedef deque_options< block_size<64 > >::type options_t;
   #endif
   typedef deque<unsigned char, void, options_t> deque_t;
   BOOST_TEST(deque_t::get_block_size() == 64U);
}

int main()
{
   test_block_bytes();
   test_block_elements();
   return ::boost::report_errors();
}
