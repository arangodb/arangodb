//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/core/lightweight_test.hpp>
#include "derived_from_memory_resource.hpp"
#include "memory_resource_logger.hpp"

using namespace boost::container::pmr;

static const std::size_t AllocCount = 32u;

namespace test_block_chain{

//explicit block_slist(memory_resource &upstream_rsrc)
void test_constructor()
{
   memory_resource_logger mrl;
   block_slist bc(mrl);
   //Resource stored
   BOOST_TEST(&bc.upstream_resource() == &mrl);
   //No allocation performed
   BOOST_TEST(mrl.m_info.size() == 0u);
}

//void *allocate(std::size_t size)
void test_allocate()
{
   memory_resource_logger mrl;
   block_slist bc(mrl);

   for(unsigned i = 0; i != unsigned(AllocCount); ++i){
      //Allocate and trace data
      const std::size_t alloc    = i+1;
      char *const addr           = (char*)bc.allocate(alloc);
      //Should have allocated a new entry
      BOOST_TEST(mrl.m_info.size() == (i+1));
      //Requested size must be bigger to include metadata
      BOOST_TEST(mrl.m_info[i].bytes > alloc);
      BOOST_TEST(mrl.m_info[i].alignment == memory_resource::max_align);
      //Returned address should be between the allocated buffer
      BOOST_TEST(mrl.m_info[i].address < addr);
      BOOST_TEST(addr < (mrl.m_info[i].address + mrl.m_info[i].bytes));
      //Allocate size should include all requested size
      BOOST_TEST((addr + alloc) <= (mrl.m_info[i].address + mrl.m_info[i].bytes));
      //Allocation must be max-aligned
      BOOST_TEST((std::size_t(addr) % memory_resource::max_align) == 0);
   }
}

//void release() BOOST_NOEXCEPT
void test_release()
{
   memory_resource_logger mrl;
   block_slist bc(mrl);

   //Allocate and trace data
   char *bufs[AllocCount];
   for(unsigned i = 0; i != unsigned(AllocCount); ++i){
      bufs[i]                    = (char*)bc.allocate(i+1);
   }
   (void)bufs;
   //Should have allocated a new entry
   BOOST_TEST(mrl.m_info.size() == AllocCount);

   //Now release and check all allocations match deallocations
   bc.release();
   BOOST_TEST(mrl.m_mismatches == 0);
   BOOST_TEST(mrl.m_info.size() == 0u);
}

//memory_resource* upstream_resource()
void test_memory_resource()
{
   derived_from_memory_resource d;
   block_slist bc(d);
   //Resource stored
   BOOST_TEST(&bc.upstream_resource() == &d);
}

//~block_slist()   {  this->release();  }
void test_destructor()
{
   memory_resource_logger mrl;
   {
      block_slist bc(mrl);

      //Allocate and trace data
      char *bufs[AllocCount];
      for(unsigned i = 0; i != unsigned(AllocCount); ++i){
         bufs[i] = (char*)bc.allocate(i+1);
      }
      (void)bufs;
      //Should have allocated a new entry
      BOOST_TEST(mrl.m_info.size() == AllocCount);

      //Destructor should release all memory
   }
   BOOST_TEST(mrl.m_mismatches == 0);
   BOOST_TEST(mrl.m_info.size() == 0u);
}

}  //namespace test_block_chain {

void test_resource_constructor()
{
   //First constructor, null resource
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      monotonic_buffer_resource m;
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(0);
   }
   //First constructor, non-null resource
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      monotonic_buffer_resource m(&dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.next_buffer_size() == monotonic_buffer_resource::initial_next_buffer_size);
      BOOST_TEST(m.current_buffer() == 0);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
}

void test_initial_size_constructor()
{
   //Second constructor, null resource
   const std::size_t initial_size = monotonic_buffer_resource::initial_next_buffer_size*2;
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      monotonic_buffer_resource m(initial_size);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.next_buffer_size() >= initial_size);
      BOOST_TEST(m.current_buffer() == 0);
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(0);
   }
   //Second constructor, non-null resource
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      monotonic_buffer_resource m(initial_size, &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.next_buffer_size() >= initial_size);
      BOOST_TEST(m.current_buffer() == 0);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
}

void test_buffer_constructor()
{
   const std::size_t BufSz = monotonic_buffer_resource::initial_next_buffer_size*2;
   unsigned char buf[BufSz];
   //Third constructor, null resource
   {
      memory_resource_logger mrl;
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(&mrl);
      monotonic_buffer_resource m(buf, BufSz);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.next_buffer_size() >= BufSz*2);
      BOOST_TEST(m.current_buffer() == buf);
      //test it does not allocate any memory
      BOOST_TEST(mrl.m_info.size() == 0u);
      set_default_resource(0);
   }
   //Third constructor, non-null resource
   {
      derived_from_memory_resource dmr;
      dmr.reset();
      monotonic_buffer_resource m(buf, sizeof(buf), &dmr);
      //test postconditions
      BOOST_TEST(m.upstream_resource() == &dmr);
      BOOST_TEST(m.next_buffer_size() >= sizeof(buf)*2);
      BOOST_TEST(m.current_buffer() == buf);
      //test it does not allocate any memory
      BOOST_TEST(dmr.do_allocate_called == false);
   }
   //Check for empty buffers
   {
      monotonic_buffer_resource m(buf, 0);
      BOOST_TEST(m.upstream_resource() == get_default_resource());
      BOOST_TEST(m.next_buffer_size() > 1);
      BOOST_TEST(m.current_buffer() == buf);
   }
}

struct derived_from_monotonic_buffer_resource
   : public monotonic_buffer_resource
{
   explicit derived_from_monotonic_buffer_resource(memory_resource *p)
      : monotonic_buffer_resource(p)
   {}

   explicit derived_from_monotonic_buffer_resource(std::size_t initial_size, memory_resource* upstream)
      : monotonic_buffer_resource(initial_size, upstream)
   {}

   explicit derived_from_monotonic_buffer_resource(void* buffer, std::size_t buffer_size, memory_resource* upstream)
      : monotonic_buffer_resource(buffer, buffer_size, upstream)
   {}

   using monotonic_buffer_resource::do_allocate;
   using monotonic_buffer_resource::do_deallocate;
   using monotonic_buffer_resource::do_is_equal;
};

void test_upstream_resource()
{
   //Allocate buffer first to avoid stack-use-after-scope in monotonic_buffer_resource's destructor
   const std::size_t BufSz = monotonic_buffer_resource::initial_next_buffer_size;
   boost::move_detail::aligned_storage<BufSz+block_slist::header_size>::type buf;
   //Test stores the resource and uses it to allocate memory
   derived_from_memory_resource dmr;
   dmr.reset();
   derived_from_monotonic_buffer_resource dmbr(&dmr);
   //Resource must be stored and initial values given (no current buffer)
   BOOST_TEST(dmbr.upstream_resource() == &dmr);
   BOOST_TEST(dmbr.next_buffer_size() == monotonic_buffer_resource::initial_next_buffer_size);
   BOOST_TEST(dmbr.current_buffer() == 0);
   //Test it does not allocate any memory
   BOOST_TEST(dmr.do_allocate_called == false);
   //Now stub buffer storage it as the return buffer
   //for "derived_from_memory_resource":
   dmr.do_allocate_return = &buf;
   //Test that allocation uses the upstream_resource()
   void *addr = dmbr.do_allocate(1u, 1u);
   //Test returns stubbed memory with the internal initial size plus metadata size
   BOOST_TEST(addr > (char*)&buf);
   BOOST_TEST(addr < (char*)(&buf+1));
   BOOST_TEST(dmr.do_allocate_called == true);
   BOOST_TEST(dmr.do_allocate_bytes > BufSz);
   //Alignment for the resource must be max_align
   BOOST_TEST(dmr.do_allocate_alignment == memory_resource::max_align);
}

void test_do_allocate()
{
   memory_resource_logger mrl;
   {
      std::size_t remaining_storage = 0u;
      derived_from_monotonic_buffer_resource dmbr(&mrl);
      //First test, no buffer
      {
         dmbr.do_allocate(1, 1);
         //It should allocate initial size
         BOOST_TEST(mrl.m_info.size() == 1u);
         //... which requests the initial size plus the header size to the allcoator
         BOOST_TEST(mrl.m_info[0].bytes == monotonic_buffer_resource::initial_next_buffer_size+block_slist::header_size);
         std::size_t remaining = dmbr.remaining_storage(1u);
         //Remaining storage should be one less than initial, as we requested 1 byte with minimal alignment
         BOOST_TEST(remaining == monotonic_buffer_resource::initial_next_buffer_size-1u);
         remaining_storage = remaining;
      }
      //Now ask for more internal storage with misaligned current buffer
      {
         //Test wasted space
         std::size_t wasted_due_to_alignment;
         dmbr.remaining_storage(4u, wasted_due_to_alignment);
         BOOST_TEST(wasted_due_to_alignment == 3u);
         dmbr.do_allocate(4, 4);
         //It should not have allocated
         BOOST_TEST(mrl.m_info.size() == 1u);
         std::size_t remaining = dmbr.remaining_storage(1u);
         //We wasted some bytes due to alignment plus 4 bytes of real storage
         BOOST_TEST(remaining == remaining_storage - 4 - wasted_due_to_alignment);
         remaining_storage = remaining;
      }
      //Now request the same alignment to test no storage is wasted
      {
         std::size_t wasted_due_to_alignment;
         std::size_t remaining = dmbr.remaining_storage(1u, wasted_due_to_alignment);
         BOOST_TEST(mrl.m_info.size() == 1u);
         dmbr.do_allocate(4, 4);
         //It should not have allocated
         BOOST_TEST(mrl.m_info.size() == 1u);
         remaining = dmbr.remaining_storage(1u);
         //We wasted no bytes due to alignment plus 4 bytes of real storage
         BOOST_TEST(remaining == remaining_storage - 4u);
         remaining_storage = remaining;
      }
      //Now exhaust the remaining storage with 2 byte alignment (the last allocation
      //was 4 bytes with 4 byte alignment) so it should be already 2-byte aligned.
      {
         dmbr.do_allocate(remaining_storage, 2);
         std::size_t wasted_due_to_alignment;
         std::size_t remaining = dmbr.remaining_storage(1u, wasted_due_to_alignment);
         BOOST_TEST(wasted_due_to_alignment == 0u);
         BOOST_TEST(remaining == 0u);
         //It should not have allocated
         BOOST_TEST(mrl.m_info.size() == 1u);
         remaining_storage = 0u;
      }
      //The next allocation should trigger the upstream resource, even with a 1 byte
      //allocation.
      {
         dmbr.do_allocate(1u, 1u);
         BOOST_TEST(mrl.m_info.size() == 2u);
         //The next allocation should be geometrically bigger.
         BOOST_TEST(mrl.m_info[1].bytes == 2*monotonic_buffer_resource::initial_next_buffer_size+block_slist::header_size);
         std::size_t wasted_due_to_alignment;
         //For a 2 byte alignment one byte will be wasted from the previous 1 byte allocation
         std::size_t remaining = dmbr.remaining_storage(2u, wasted_due_to_alignment);
         BOOST_TEST(wasted_due_to_alignment == 1u);
         BOOST_TEST(remaining == (mrl.m_info[1].bytes - 1u  - wasted_due_to_alignment - block_slist::header_size));
         //It should not have allocated
         remaining_storage = dmbr.remaining_storage(1u);
      }
      //Now try a bigger than next allocation and see if next_buffer_size is doubled.
      {
         std::size_t next_alloc = 5*monotonic_buffer_resource::initial_next_buffer_size;
         dmbr.do_allocate(next_alloc, 1u);
         BOOST_TEST(mrl.m_info.size() == 3u);
         //The next allocation should be geometrically bigger.
         BOOST_TEST(mrl.m_info[2].bytes == 8*monotonic_buffer_resource::initial_next_buffer_size+block_slist::header_size);
         remaining_storage = dmbr.remaining_storage(1u);
      }
   }
   //derived_from_monotonic_buffer_resource dmbr(&mrl) is destroyed
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);

   //Now use a local buffer
   {
      boost::move_detail::aligned_storage
         <monotonic_buffer_resource::initial_next_buffer_size>::type buf;
      //Supply an external buffer
      derived_from_monotonic_buffer_resource dmbr(&buf, sizeof(buf), &mrl);
      BOOST_TEST(dmbr.remaining_storage(1u) == sizeof(buf));
      //Allocate all remaining storage
      dmbr.do_allocate(dmbr.remaining_storage(1u), 1u);
      //No new allocation should have occurred
      BOOST_TEST(mrl.m_info.size() == 0u);
      BOOST_TEST(dmbr.remaining_storage(1u) == 0u);
   }
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);
}

void test_do_deallocate()
{
   memory_resource_logger mrl;
   const std::size_t initial_size = 1u;
   {
      derived_from_monotonic_buffer_resource dmbr(initial_size, &mrl);
      //First test, no buffer
      const unsigned iterations = 8;
      char *bufs[iterations];
      std::size_t sizes[iterations];
      //Test each iteration allocates memory
      for(unsigned i = 0; i != iterations; ++i)
      {
         sizes[i] = dmbr.remaining_storage()+1;
         bufs[i]  = (char*)dmbr.do_allocate(sizes[i], 1);
         BOOST_TEST(mrl.m_info.size() == (i+1));
      }
      std::size_t remaining = dmbr.remaining_storage();
      //Test do_deallocate does not release any storage
      for(unsigned i = 0; i != iterations; ++i)
      {
         dmbr.do_deallocate(bufs[i], sizes[i], 1u);
         BOOST_TEST(mrl.m_info.size() == iterations);
         BOOST_TEST(remaining == dmbr.remaining_storage());
         BOOST_TEST(mrl.m_mismatches == 0u);
      }
   }
}

void test_do_is_equal()
{
   //! <b>Returns</b>:
   //!   `this == dynamic_cast<const monotonic_buffer_resource*>(&other)`.
   memory_resource_logger mrl;
   derived_from_monotonic_buffer_resource dmbr(&mrl);
   derived_from_monotonic_buffer_resource dmbr2(&mrl);
   BOOST_TEST(true == dmbr.do_is_equal(dmbr));
   BOOST_TEST(false == dmbr.do_is_equal(dmbr2));
   //A different type should be always different
   derived_from_memory_resource dmr;
   BOOST_TEST(false == dmbr.do_is_equal(dmr));
}

void test_release()
{
   {
      memory_resource_logger mrl;
      const std::size_t initial_size = 1u;
      derived_from_monotonic_buffer_resource dmbr(initial_size, &mrl);
      //First test, no buffer
      const unsigned iterations = 8;
      //Test each iteration allocates memory
      for(unsigned i = 0; i != iterations; ++i)
      {
         dmbr.do_allocate(dmbr.remaining_storage()+1, 1);
         BOOST_TEST(mrl.m_info.size() == (i+1));
      }
      //Release and check memory was released
      dmbr.release();
      BOOST_TEST(mrl.m_mismatches == 0u);
      BOOST_TEST(mrl.m_info.size() == 0u);
   }
   //Now use a local buffer
   {
      boost::move_detail::aligned_storage
         <monotonic_buffer_resource::initial_next_buffer_size>::type buf;
      //Supply an external buffer
      monotonic_buffer_resource monr(&buf, sizeof(buf));
      memory_resource &mr = monr;
      BOOST_TEST(monr.remaining_storage(1u) == sizeof(buf));
      //Allocate all remaining storage
      mr.allocate(monr.remaining_storage(1u), 1u);
      BOOST_TEST(monr.current_buffer() == ((char*)&buf + sizeof(buf)));
      //No new allocation should have occurred
      BOOST_TEST(monr.remaining_storage(1u) == 0u);
      //Release and check memory was released and the original buffer is back
      monr.release();
      BOOST_TEST(monr.remaining_storage(1u) == sizeof(buf));
      BOOST_TEST(monr.current_buffer() == &buf);
   }
}

void test_destructor()
{
   memory_resource_logger mrl;
   const std::size_t initial_size = 1u;
   {
      derived_from_monotonic_buffer_resource dmbr(initial_size, &mrl);
      //First test, no buffer
      const unsigned iterations = 8;
      //Test each iteration allocates memory
      for(unsigned i = 0; i != iterations; ++i)
      {
         dmbr.do_allocate(dmbr.remaining_storage()+1, 1);
         BOOST_TEST(mrl.m_info.size() == (i+1));
      }
   }  //dmbr is destroyed, memory should be released
   BOOST_TEST(mrl.m_mismatches == 0u);
   BOOST_TEST(mrl.m_info.size() == 0u);
}

int main()
{
   test_block_chain::test_constructor();
   test_block_chain::test_allocate();
   test_block_chain::test_release();
   test_block_chain::test_memory_resource();
   test_block_chain::test_destructor();

   test_resource_constructor();
   test_initial_size_constructor();
   test_buffer_constructor();

   test_upstream_resource();
   test_do_allocate();
   test_do_deallocate();
   test_do_is_equal();
   test_release();
   test_destructor();
   return ::boost::report_errors();
}
