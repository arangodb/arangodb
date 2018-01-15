//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2016-2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//#define BOOST_MOVE_ADAPTIVE_SORT_INVARIANTS
#define BOOST_MOVE_ADAPTIVE_SORT_STATS

#include "order_type.hpp"

#include <iostream>  //std::cout
#include <boost/config.hpp>

#include <boost/move/algo/detail/adaptive_sort_merge.hpp>
#include <boost/move/core.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/move/make_unique.hpp>

#include <boost/move/detail/type_traits.hpp>
#include <boost/core/lightweight_test.hpp>

#include <cstddef>

const std::size_t BlockSize = 7u;

#if defined(BOOST_MSVC)
#pragma warning (disable : 4267)
#endif


const std::size_t left_merge = 0;
const std::size_t buf_merge  = 1;
const std::size_t unbuf_merge= 2;
const std::size_t max_merge  = 3;

template<class Op>
void alternating_test(
   const std::size_t NumBlocksA,
   const std::size_t NumBlocksB,
   const std::size_t ExtraA,
   const std::size_t ExtraB,
   Op op)
{
   using namespace boost::movelib::detail_adaptive;


   const std::size_t DataSize   = ExtraA + NumBlocksA*BlockSize + NumBlocksB*BlockSize + ExtraB;
   const std::size_t KeySize    = NumBlocksA + NumBlocksB + 1;
   const std::size_t HdrSize    = BlockSize + KeySize;
   const std::size_t ArraySize  = HdrSize + DataSize;

   boost::movelib::unique_ptr<order_move_type[]> testarray(boost::movelib::make_unique<order_move_type[]>(ArraySize));


   for(std::size_t szt_merge = 0; szt_merge != max_merge; ++szt_merge){
      //Order keys
      for (std::size_t szt_i = 0u; szt_i != KeySize; ++szt_i) {
         testarray[szt_i].key = szt_i;
         testarray[szt_i].val = std::size_t(-1);
      }

      //Order buffer
      for (std::size_t szt_i = 0u; szt_i != BlockSize; ++szt_i) {
         testarray[KeySize+szt_i].key = std::size_t(-1);
         testarray[KeySize+szt_i].val = szt_i;
      }

      //Block A
      std::size_t szt_k = 0;
      for (std::size_t szt_i = 0u; szt_i != ExtraA;  ++szt_i) {
         testarray[HdrSize+szt_k].key = (szt_k/2)*2;
         testarray[HdrSize+szt_k].val = szt_k & 1;
         ++szt_k;
      }

      for (std::size_t szt_b = 0u; szt_b != NumBlocksA; ++szt_b)
      for (std::size_t szt_i = 0u; szt_i != BlockSize;  ++szt_i) {
         testarray[HdrSize+szt_k].key = (szt_k/2)*2;
         testarray[HdrSize+szt_k].val = szt_k & 1;
         ++szt_k;
      }

      //Block B
      std::size_t szt_l = 0;
      for (std::size_t szt_b = 0u, szt_t = 0; szt_b != NumBlocksB; ++szt_b)
      for (std::size_t szt_i = 0u; szt_i != BlockSize;  ++szt_i, ++szt_t) {
         testarray[HdrSize+szt_k].key = (szt_l/2)*2+1;
         testarray[HdrSize+szt_k].val = szt_l & 1;
         ++szt_k;
         ++szt_l;
      }

      for (std::size_t szt_i = 0u; szt_i != ExtraB;  ++szt_i) {
         testarray[HdrSize+szt_k].key = (szt_l/2)*2+1;
         testarray[HdrSize+szt_k].val = szt_l & 1;
         ++szt_k;
         ++szt_l;
      }

      if(szt_merge == left_merge){
         //Merge Left
         op_merge_blocks_left
            ( testarray.get(), order_type_less()
            , testarray.get()+HdrSize, BlockSize, ExtraA, NumBlocksA, NumBlocksB, ExtraB
            , order_type_less(), op );
         BOOST_TEST( is_order_type_ordered(testarray.get()+KeySize, DataSize) );
         BOOST_TEST( is_key(testarray.get(), KeySize) );
         BOOST_TEST(( !boost::move_detail::is_same<Op, boost::movelib::swap_op>::value
                     || is_buffer(testarray.get()+ KeySize+DataSize, BlockSize) ));
      }
      else if(szt_merge == buf_merge){
         //Merge with buf
         op_merge_blocks_with_buf
            ( testarray.get(), order_type_less()
            , testarray.get()+HdrSize, BlockSize, ExtraA, NumBlocksA, NumBlocksB, ExtraB
            , order_type_less(), op, testarray.get()+KeySize );
         BOOST_TEST( is_order_type_ordered(testarray.get()+HdrSize, DataSize) );
         BOOST_TEST( is_key(testarray.get(), KeySize) );
         BOOST_TEST(( !boost::move_detail::is_same<Op, boost::movelib::swap_op>::value
                     || is_buffer(testarray.get()+ KeySize, BlockSize) ));
      }
      else if(szt_merge == unbuf_merge){
         //Merge Left
         merge_blocks_bufferless
            ( testarray.get(), order_type_less()
            , testarray.get()+HdrSize, BlockSize, ExtraA, NumBlocksA, NumBlocksB, ExtraB
            , order_type_less());
         BOOST_TEST( is_order_type_ordered(testarray.get()+HdrSize, DataSize) );
         BOOST_TEST( is_key(testarray.get(), KeySize) );
         BOOST_TEST(( !boost::move_detail::is_same<Op, boost::movelib::swap_op>::value
                     || is_buffer(testarray.get()+ KeySize, BlockSize) ));
      }
   }
}

int main()
{
   {
      const std::size_t NumBlocksA = 3u;
      const std::size_t NumBlocksB = 3u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = ExtraA;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 3u;
      const std::size_t NumBlocksB = 3u;
      const std::size_t ExtraA     = 0u;
      const std::size_t ExtraB     = BlockSize/2;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 3u;
      const std::size_t NumBlocksB = 3u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 3u;
      const std::size_t NumBlocksB = 3u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 6u;
      const std::size_t NumBlocksB = 3u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = ExtraA;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 6u;
      const std::size_t NumBlocksB = 3u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 3u;
      const std::size_t NumBlocksB = 5u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = ExtraA;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 3u;
      const std::size_t NumBlocksB = 5u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 0u;
      const std::size_t NumBlocksB = 0u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 0u;
      const std::size_t NumBlocksB = 0u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 0u;
      const std::size_t NumBlocksB = 0u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = BlockSize/2;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   //
   {
      const std::size_t NumBlocksA = 0u;
      const std::size_t NumBlocksB = 1u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 1u;
      const std::size_t NumBlocksB = 0u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 1u;
      const std::size_t NumBlocksB = 0u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 0u;
      const std::size_t NumBlocksB = 1u;
      const std::size_t ExtraA     = BlockSize/2;
      const std::size_t ExtraB     = 0;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 1u;
      const std::size_t NumBlocksB = 0u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = BlockSize/2;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }
   {
      const std::size_t NumBlocksA = 0u;
      const std::size_t NumBlocksB = 1u;
      const std::size_t ExtraA     = 0;
      const std::size_t ExtraB     = BlockSize/2;
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::move_op());
      alternating_test(NumBlocksA, NumBlocksB, ExtraA, ExtraB, boost::movelib::swap_op());
   }

   return ::boost::report_errors();
}
