//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/pmr/global_resource.hpp>
#include <boost/core/lightweight_test.hpp>

#include <boost/intrusive/detail/math.hpp>

#include "derived_from_memory_resource.hpp"
#include "memory_resource_logger.hpp"

using namespace boost::container::pmr;

template<class PoolResource>
struct derived_from_pool_resource
   : public PoolResource
{
    derived_from_pool_resource(const pool_options& opts, memory_resource* upstream)
      : PoolResource(opts, upstream)
   {}

  explicit derived_from_pool_resource(memory_resource *p)
      : PoolResource(p)
   {}

   explicit derived_from_pool_resource(const pool_options &opts)
      : PoolResource(opts)
   {}

   derived_from_pool_resource()
      : PoolResource()
   {}

   using PoolResource::do_allocate;
   using PoolResource::do_deallocate;
   using PoolResource::do_is_equal;
};

template<class PoolResource>
void test_default_constructor()
{
   //With default options/resource
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      PoolResource m;
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_default_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
}

template<class PoolResource>
void test_upstream_constructor()
{
   //With a resource, default options
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      PoolResource m(&dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_default_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
}

template<class PoolResource>
void test_options_constructor()
{
   //Default options
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      pool_options opts;
      PoolResource m(opts);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_default_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
   }
   //Too large option values
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      pool_options opts;
      opts.max_blocks_per_chunk = pool_options_default_max_blocks_per_chunk+1;
      opts.largest_required_pool_block = pool_options_default_largest_required_pool_block+1;
      PoolResource m(opts);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_default_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
   }
   //Too small option values
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      pool_options opts;
      opts.largest_required_pool_block = pool_options_minimum_largest_required_pool_block-1u;
      PoolResource m(opts);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_minimum_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
   }
   //In range option values
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      pool_options opts;
      opts.max_blocks_per_chunk = pool_options_default_max_blocks_per_chunk;
      opts.largest_required_pool_block = pool_options_minimum_largest_required_pool_block;
      PoolResource m(opts);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_minimum_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
   }
}

template<class PoolResource>
void test_options_upstream_constructor()
{
   //Default options
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      pool_options opts;
      PoolResource m(opts, &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_default_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
   //Too large option values
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      pool_options opts;
      opts.max_blocks_per_chunk = pool_options_default_max_blocks_per_chunk+1;
      opts.largest_required_pool_block = pool_options_default_largest_required_pool_block+1;
      PoolResource m(opts, &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_default_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
   //Too small option values
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      pool_options opts;
      opts.largest_required_pool_block = pool_options_minimum_largest_required_pool_block-1u;
      PoolResource m(opts, &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      BOOST_TEST(m.options().largest_required_pool_block == pool_options_minimum_largest_required_pool_block);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
   //In range option values
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      pool_options opts;
      opts.max_blocks_per_chunk = pool_options_default_max_blocks_per_chunk;
      opts.largest_required_pool_block = pool_options_minimum_largest_required_pool_block;
      PoolResource m(opts, &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      //max blocks is unchanged in this implementation
      BOOST_TEST(m.options().max_blocks_per_chunk == pool_options_default_max_blocks_per_chunk);
      //largest block is rounded to pow2
      BOOST_TEST(m.options().largest_required_pool_block == bi::detail::ceil_pow2(opts.largest_required_pool_block));
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
}

template<class PoolResource>
void test_options()
{
   //In range option values
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      pool_options opts;
      opts.max_blocks_per_chunk = pool_options_default_max_blocks_per_chunk/2u;
      opts.largest_required_pool_block = (pool_options_default_largest_required_pool_block
         - pool_options_minimum_largest_required_pool_block) | std::size_t(1); //guaranteed to be non power of 2.
      PoolResource m(opts, &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      //max blocks is unchanged in this implementation
      BOOST_TEST(m.options().max_blocks_per_chunk == opts.max_blocks_per_chunk);
      //largest block is rounded to pow2
      BOOST_TEST(m.options().largest_required_pool_block == bi::detail::ceil_pow2(opts.largest_required_pool_block));
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
}

template<class PoolResource>
void test_do_allocate_deallocate()
{
   memory_resource_logger mrl;
   {
      derived_from_pool_resource<PoolResource> dmbr(&mrl);
      {
         //First block from pool 0
         dmbr.do_allocate(1, 1);
         //It should allocate the pool array plus an initial block
         BOOST_TEST(mrl.m_info.size() == 2u);
         //Second block from pool 0
         dmbr.do_allocate(1, 1);
         //It should allocate again (with 2 chunks per block)
         BOOST_TEST(mrl.m_info.size() == 3u);
         //Third block from pool 0
         dmbr.do_allocate(1, 1);
         //It should NOT allocate again (previous was a 2 block chunk)
         BOOST_TEST(mrl.m_info.size() == 3u);
      }
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);

   //Allocate and deallocate from the same chunk to test block caching
   {
      derived_from_pool_resource<PoolResource> dmbr(&mrl);
      {
         //First block from pool 0
         void *p = dmbr.do_allocate(1, 1);
         //It should allocate the pool array plus an initial block
         BOOST_TEST(mrl.m_info.size() == 2u);
         //No cached, as initial blocks per chunk is 1
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == 0u);
         //Deallocate and allocate again
         dmbr.do_deallocate(p, 1, 1);
         //Cached
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == 1u);
         p = dmbr.do_allocate(1, 1);
         //Reused
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == 0u);
         //It should have NOT allocated (block reuse)
         BOOST_TEST(mrl.m_info.size() == 2u);

         //Allocate again 2 times (a 2 block chunk is exhausted)
         void *p2 = dmbr.do_allocate(1, 1);
         //1 left cached
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == 1u);
         void *p3 = dmbr.do_allocate(1, 1);
         //Cache exhausted
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == 0u);
         //Single chunk allocation happened
         BOOST_TEST(mrl.m_info.size() == 3u);

         //Now deallocate all (no memory is freed, all cached)
         dmbr.do_deallocate(p2, 1, 1);
         dmbr.do_deallocate(p3, 1, 1);
         dmbr.do_deallocate(p, 1, 1);
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == 3u);
         BOOST_TEST(mrl.m_info.size() == 3u);
      }
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);

   //Now test max block per chunk
   {
      pool_options opts;
      //so after max_blocks_per_chunk*2-1 allocations, all new chunks must hold max_blocks_per_chunk blocks
      opts.max_blocks_per_chunk = 32u; 
      derived_from_pool_resource<PoolResource> dmbr(opts, &mrl);
      {
         std::size_t loops = opts.max_blocks_per_chunk*2-1u;
         while(loops--){
            dmbr.do_allocate(1, 1);
         }
         //pool array + log2(max_blocks_per_chunk)+1 chunks (sizes [1, 2, 4, ...])
         const std::size_t num_chunks = bi::detail::floor_log2(opts.max_blocks_per_chunk)+1u;
         BOOST_TEST(mrl.m_info.size() == 1u + num_chunks);
         //Next allocation should allocate max_blocks_per_chunk blocks in a chunk so max_blocks_per_chunk-1 should remain free
         dmbr.do_allocate(1, 1);
         BOOST_TEST(mrl.m_info.size() == 1u + num_chunks + 1u);
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == (opts.max_blocks_per_chunk-1u));
         //Exhaust the chunk and allocate a new one, test max_blocks_per_chunk is not passed again
         loops = opts.max_blocks_per_chunk;
         while(loops--){
            dmbr.do_allocate(1, 1);
         }
         BOOST_TEST(mrl.m_info.size() == 1u + num_chunks + 2u);
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == (opts.max_blocks_per_chunk-1u));
      }
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);

   //Now test max block per chunk
   {
      pool_options opts;
      //so after max_blocks_per_chunk*2-1 allocations, all new chunks must hold max_blocks_per_chunk blocks
      opts.max_blocks_per_chunk = 32u; 
      derived_from_pool_resource<PoolResource> dmbr(opts, &mrl);
      {
         std::size_t loops = opts.max_blocks_per_chunk*2-1u;
         while(loops--){
            dmbr.do_allocate(1, 1);
         }
         //pool array + log2(max_blocks_per_chunk)+1 chunks (sizes [1, 2, 4, ...])
         BOOST_TEST(dmbr.pool_next_blocks_per_chunk(0u) == opts.max_blocks_per_chunk);
         const std::size_t num_chunks = bi::detail::floor_log2(opts.max_blocks_per_chunk)+1u;
         BOOST_TEST(mrl.m_info.size() == 1u + num_chunks);
         //Next allocation should allocate max_blocks_per_chunk blocks in a chunk so max_blocks_per_chunk-1 should remain free
         dmbr.do_allocate(1, 1);
         BOOST_TEST(dmbr.pool_next_blocks_per_chunk(0u) == opts.max_blocks_per_chunk);
         BOOST_TEST(mrl.m_info.size() == 1u + num_chunks + 1u);
         BOOST_TEST(dmbr.pool_cached_blocks(0u) == (opts.max_blocks_per_chunk-1u));
      }
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);

   //Now test different pool sizes
   {
      pool_options opts;
      //so after max_blocks_per_chunk*2-1 allocations, all new chunks must hold max_blocks_per_chunk blocks
      opts.max_blocks_per_chunk = 1u; 
      derived_from_pool_resource<PoolResource> dmbr(opts, &mrl);
      const pool_options &final_opts = dmbr.options();

      //Force pool creation
      dmbr.do_deallocate(dmbr.do_allocate(1, 1), 1, 1);
      //pool array plus first pool's chunk allocation
      BOOST_TEST(mrl.m_info.size() == 2u);
      //pool count must be:
      // log2(the maximum block) - log2(the minimum block) + 1. Example if minimum block is 8, and maximum 32:
      // log(32) - log2(8) + 1u = 3 pools (block sizes: 8, 16, and 32)
      const std::size_t minimum_size = dmbr.pool_block(0u);
      const std::size_t maximum_size = final_opts.largest_required_pool_block;
      BOOST_TEST(dmbr.pool_count() == (1u + bi::detail::floor_log2(maximum_size) - bi::detail::floor_log2(minimum_size)));
      for(std::size_t i = 0, s = minimum_size, max = dmbr.pool_count(); i != max; ++i, s*=2){
         //Except in the first pool, each cache should be empty
         BOOST_TEST(dmbr.pool_cached_blocks(i) == std::size_t(i == 0));
         dmbr.do_deallocate(dmbr.do_allocate(s/2+1, 1), s/2+1, 1);
         dmbr.do_deallocate(dmbr.do_allocate(s-1, 1), s-1, 1);
         dmbr.do_deallocate(dmbr.do_allocate(s, 1), s, 1);
         //pool array plus each previous chunk allocation
         BOOST_TEST(mrl.m_info.size() == (1u + i + 1u));
         //as we limited max_blocks_per_chunk to 1, no cached blocks should be available except one
         BOOST_TEST(dmbr.pool_cached_blocks(i) == 1u);
      }
      //Now test out of maximum values, which should go directly to upstream
      //it should be directly deallocated.
      void *p = dmbr.do_allocate(maximum_size+1, 1);
      BOOST_TEST(mrl.m_info.size() == (1u + dmbr.pool_count() + 1u));
      dmbr.do_deallocate(p, maximum_size+1, 1);
      BOOST_TEST(mrl.m_info.size() == (1u + dmbr.pool_count()));
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);
}

template<class PoolResource>
void test_do_is_equal()
{
   //`this == dynamic_cast<const PoolResource*>(&other)`.
   memory_resource_logger mrl;
   derived_from_pool_resource<PoolResource> dmbr(&mrl);
   derived_from_pool_resource<PoolResource> dmbr2(&mrl);
   BOOST_TEST(true == dmbr.do_is_equal(dmbr));
   BOOST_TEST(false == dmbr.do_is_equal(dmbr2));
   //A different type should be always different
   derived_from_memory_resource dmr;
   BOOST_TEST(false == dmbr.do_is_equal(dmr));
}

template<class PoolResource>
void test_release()
{
   memory_resource_logger mrl;
   {
      pool_options opts;
      //so after max_blocks_per_chunk*2-1 allocations, all new chunks must hold max_blocks_per_chunk blocks
      opts.max_blocks_per_chunk = 4u; 
      derived_from_pool_resource<PoolResource> dmbr(opts, &mrl);
      const pool_options &final_opts = dmbr.options();
      const std::size_t minimum_size = dmbr.pool_block(0u);
      const std::size_t maximum_size = final_opts.largest_required_pool_block;
      const std::size_t pool_count = 1u + bi::detail::floor_log2(maximum_size) - bi::detail::floor_log2(minimum_size);

      std::size_t expected_memory_allocs = 0;
      for(std::size_t i = 0, imax = pool_count, s = minimum_size; i != imax; s*=2, ++i){
         for(std::size_t j = 0, j_max = opts.max_blocks_per_chunk*2u-1u; j != j_max; ++j){
            dmbr.do_allocate(s, 1);
         }
         //One due to the pool array, and for each pool, log2(max_blocks_per_chunk)+1 allocations
         expected_memory_allocs = 1 + (bid::floor_log2(opts.max_blocks_per_chunk) + 1u)*(i+1);
         //pool array plus each previous chunk allocation
         BOOST_TEST(mrl.m_info.size() == expected_memory_allocs);
      }
      //Now with out-of-pool sizes
      for(std::size_t j = 0, j_max = opts.max_blocks_per_chunk*2u-1u; j != j_max; ++j){
         dmbr.do_allocate(maximum_size+1, 1);
         BOOST_TEST(mrl.m_info.size() == ++expected_memory_allocs);
      }
      //Now release memory and check all memory allocated through do_allocate was deallocated to upstream      
      dmbr.release();
      BOOST_TEST(mrl.m_info.size() == 1u);
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);
}

template<class PoolResource>
void test_destructor()
{
   memory_resource_logger mrl;
   {
      pool_options opts;
      //so after max_blocks_per_chunk*2-1 allocations, all new chunks must hold max_blocks_per_chunk blocks
      opts.max_blocks_per_chunk = 4u; 
      derived_from_pool_resource<PoolResource> dmbr(opts, &mrl);
      const pool_options &final_opts = dmbr.options();
      const std::size_t minimum_size = dmbr.pool_block(0u);
      const std::size_t maximum_size = final_opts.largest_required_pool_block;
      const std::size_t pool_count = 1u + bi::detail::floor_log2(maximum_size) - bi::detail::floor_log2(minimum_size);

      std::size_t expected_memory_allocs = 0;
      for(std::size_t i = 0, imax = pool_count, s = minimum_size; i != imax; s*=2, ++i){
         for(std::size_t j = 0, j_max = opts.max_blocks_per_chunk*2u-1u; j != j_max; ++j){
            dmbr.do_allocate(s, 1);
         }
         //One due to the pool array, and for each pool, log2(max_blocks_per_chunk)+1 allocations
         expected_memory_allocs = 1 + (bid::floor_log2(opts.max_blocks_per_chunk) + 1u)*(i+1);
         //pool array plus each previous chunk allocation
         BOOST_TEST(mrl.m_info.size() == expected_memory_allocs);
      }
      //Now with out-of-pool sizes
      for(std::size_t j = 0, j_max = opts.max_blocks_per_chunk*2u-1u; j != j_max; ++j){
         dmbr.do_allocate(maximum_size+1, 1);
         BOOST_TEST(mrl.m_info.size() == ++expected_memory_allocs);
      }
      //Don't release, all memory, including internal allocations, should be automatically
      //after the destructor is run
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);
}


template<class PoolResource>
void test_pool_resource()
{
   test_options_upstream_constructor<PoolResource>();
   test_default_constructor<PoolResource>();
   test_upstream_constructor<PoolResource>();
   test_options_constructor<PoolResource>();
   test_options<PoolResource>();
   test_do_allocate_deallocate<PoolResource>();
   test_do_is_equal<PoolResource>();
   test_release<PoolResource>();
   test_destructor<PoolResource>();
}
