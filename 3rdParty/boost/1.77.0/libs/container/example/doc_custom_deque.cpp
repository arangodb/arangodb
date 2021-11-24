//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_custom_deque
#include <boost/container/deque.hpp>
#include <boost/static_assert.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   //This option specifies the desired block size for deque
   typedef deque_options< block_size<128u> >::type block_128_option_t;

   //This deque will allocate blocks of 128 elements
   typedef deque<int, void, block_128_option_t > block_128_deque_t;
   assert(block_128_deque_t::get_block_size() == 128u);

   //This option specifies the maximum block size for deque
   //in bytes
   typedef deque_options< block_bytes<1024u> >::type block_1024_bytes_option_t;

   //This deque will allocate blocks of 1024 bytes
   typedef deque<int, void, block_1024_bytes_option_t > block_1024_bytes_deque_t;
   assert(block_1024_bytes_deque_t::get_block_size() == 1024u/sizeof(int));

   return 0;
}
//]
