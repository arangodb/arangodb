//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/config_begin.hpp>
#include <boost/container/scoped_allocator_fwd.hpp>

// container/detail
#include <boost/container/detail/mpl.hpp>
// move
#include <boost/move/utility_core.hpp>
#include <boost/move/adl_move_swap.hpp>
//boost
#include <boost/tuple/tuple.hpp>
// std
#include <memory>
#include <cstddef>

#if defined(BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE)
#include <tuple>
#endif

//test
#include <boost/core/lightweight_test.hpp>

#include "allocator_argument_tester.hpp"

template<unsigned int Type>
struct tagged_integer
{};

struct mark_on_destructor
{
   mark_on_destructor()
   {
      destroyed = false;
   }

   ~mark_on_destructor()
   {
      destroyed = true;
   }

   static bool destroyed;
};

bool mark_on_destructor::destroyed = false;

#include <boost/container/scoped_allocator.hpp>
#include <boost/static_assert.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/detail/pair.hpp>

int main()
{
   using namespace boost::container;

   typedef propagation_test_allocator<tagged_integer<0>, 0>   OuterAlloc;
   typedef propagation_test_allocator<tagged_integer<0>, 10>  Outer10IdAlloc;
   typedef propagation_test_allocator<tagged_integer<9>, 0>   Rebound9OuterAlloc;
   typedef propagation_test_allocator<tagged_integer<1>, 1>   InnerAlloc1;
   typedef propagation_test_allocator<tagged_integer<2>, 2>   InnerAlloc2;
   typedef propagation_test_allocator<tagged_integer<1>, 11>  Inner11IdAlloc1;

   typedef propagation_test_allocator<tagged_integer<0>, 0, false>      OuterAllocFalseHasTrueTypes;
   typedef propagation_test_allocator<tagged_integer<0>, 0, true>       OuterAllocTrueHasTrueTypes;
   typedef propagation_test_allocator<tagged_integer<1>, 1, false>      InnerAlloc1FalseHasTrueTypes;
   typedef propagation_test_allocator<tagged_integer<1>, 1, true>       InnerAlloc1TrueHasTrueTypes;
   typedef propagation_test_allocator<tagged_integer<2>, 2, false>      InnerAlloc2FalseHasTrueTypes;
   typedef propagation_test_allocator<tagged_integer<2>, 2, true>       InnerAlloc2TrueHasTrueTypes;

   //
   typedef scoped_allocator_adaptor< OuterAlloc  >          Scoped0Inner;
   typedef scoped_allocator_adaptor< OuterAlloc
                                   , InnerAlloc1 >          Scoped1Inner;
   typedef scoped_allocator_adaptor< OuterAlloc
                                   , InnerAlloc1
                                   , InnerAlloc2 >          Scoped2Inner;
   typedef scoped_allocator_adaptor
      < scoped_allocator_adaptor
         <Outer10IdAlloc>
      >                                                     ScopedScoped0Inner;
   typedef scoped_allocator_adaptor
      < scoped_allocator_adaptor
         <Outer10IdAlloc, Inner11IdAlloc1>
      , InnerAlloc1
      >                                                     ScopedScoped1Inner;
   typedef scoped_allocator_adaptor< Rebound9OuterAlloc  >  Rebound9Scoped0Inner;
   typedef scoped_allocator_adaptor< Rebound9OuterAlloc
                                   , InnerAlloc1 >          Rebound9Scoped1Inner;
   typedef scoped_allocator_adaptor< Rebound9OuterAlloc
                                   , InnerAlloc1
                                   , InnerAlloc2 >          Rebound9Scoped2Inner;

   //outer_allocator_type
   BOOST_STATIC_ASSERT(( dtl::is_same< OuterAlloc
                       , Scoped0Inner::outer_allocator_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< OuterAlloc
                       , Scoped1Inner::outer_allocator_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< OuterAlloc
                       , Scoped2Inner::outer_allocator_type>::value ));
   //value_type
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::value_type
                       , Scoped0Inner::value_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::value_type
                       , Scoped1Inner::value_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::value_type
                       , Scoped2Inner::value_type>::value ));
   //size_type
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::size_type
                       , Scoped0Inner::size_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::size_type
                       , Scoped1Inner::size_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::size_type
                       , Scoped2Inner::size_type>::value ));

   //difference_type
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::difference_type
                       , Scoped0Inner::difference_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::difference_type
                       , Scoped1Inner::difference_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::difference_type
                       , Scoped2Inner::difference_type>::value ));

   //pointer
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::pointer
                       , Scoped0Inner::pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::pointer
                       , Scoped1Inner::pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::pointer
                       , Scoped2Inner::pointer>::value ));

   //const_pointer
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::const_pointer
                       , Scoped0Inner::const_pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::const_pointer
                       , Scoped1Inner::const_pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::const_pointer
                       , Scoped2Inner::const_pointer>::value ));

   //void_pointer
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::void_pointer
                       , Scoped0Inner::void_pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::void_pointer
                       , Scoped1Inner::void_pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::void_pointer
                       , Scoped2Inner::void_pointer>::value ));

   //const_void_pointer
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::const_void_pointer
                       , Scoped0Inner::const_void_pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::const_void_pointer
                       , Scoped1Inner::const_void_pointer>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< allocator_traits<OuterAlloc>::const_void_pointer
                       , Scoped2Inner::const_void_pointer>::value ));

   //rebind
   BOOST_STATIC_ASSERT(( dtl::is_same<Scoped0Inner::rebind< tagged_integer<9> >::other
                       , Rebound9Scoped0Inner >::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same<Scoped1Inner::rebind< tagged_integer<9> >::other
                       , Rebound9Scoped1Inner >::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same<Scoped2Inner::rebind< tagged_integer<9> >::other
                       , Rebound9Scoped2Inner >::value ));

   //inner_allocator_type
   BOOST_STATIC_ASSERT(( dtl::is_same< Scoped0Inner
                       , Scoped0Inner::inner_allocator_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< scoped_allocator_adaptor<InnerAlloc1>
                       , Scoped1Inner::inner_allocator_type>::value ));
   BOOST_STATIC_ASSERT(( dtl::is_same< scoped_allocator_adaptor<InnerAlloc1, InnerAlloc2>
                       , Scoped2Inner::inner_allocator_type>::value ));

   {
      //Propagation test
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes  >  Scoped0InnerF;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes  >   Scoped0InnerT;
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes
                                    , InnerAlloc1FalseHasTrueTypes >  Scoped1InnerFF;
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes
                                    , InnerAlloc1TrueHasTrueTypes >   Scoped1InnerFT;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes
                                    , InnerAlloc1FalseHasTrueTypes >  Scoped1InnerTF;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes
                                    , InnerAlloc1TrueHasTrueTypes >   Scoped1InnerTT;
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes
                                    , InnerAlloc1FalseHasTrueTypes
                                    , InnerAlloc2FalseHasTrueTypes >  Scoped2InnerFFF;
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes
                                    , InnerAlloc1FalseHasTrueTypes
                                    , InnerAlloc2TrueHasTrueTypes >  Scoped2InnerFFT;
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes
                                    , InnerAlloc1TrueHasTrueTypes
                                    , InnerAlloc2FalseHasTrueTypes >  Scoped2InnerFTF;
      typedef scoped_allocator_adaptor< OuterAllocFalseHasTrueTypes
                                    , InnerAlloc1TrueHasTrueTypes
                                    , InnerAlloc2TrueHasTrueTypes >  Scoped2InnerFTT;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes
                                    , InnerAlloc1FalseHasTrueTypes
                                    , InnerAlloc2FalseHasTrueTypes >  Scoped2InnerTFF;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes
                                    , InnerAlloc1FalseHasTrueTypes
                                    , InnerAlloc2TrueHasTrueTypes >  Scoped2InnerTFT;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes
                                    , InnerAlloc1TrueHasTrueTypes
                                    , InnerAlloc2FalseHasTrueTypes >  Scoped2InnerTTF;
      typedef scoped_allocator_adaptor< OuterAllocTrueHasTrueTypes
                                    , InnerAlloc1TrueHasTrueTypes
                                    , InnerAlloc2TrueHasTrueTypes >  Scoped2InnerTTT;

      //propagate_on_container_copy_assignment
      //0 inner
      BOOST_STATIC_ASSERT(( !Scoped0InnerF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped0InnerT::propagate_on_container_copy_assignment::value ));
      //1 inner
      BOOST_STATIC_ASSERT(( !Scoped1InnerFF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerFT::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTT::propagate_on_container_copy_assignment::value ));
      //2 inner
      BOOST_STATIC_ASSERT(( !Scoped2InnerFFF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFFT::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFTF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFTT::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTFF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTFT::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTF::propagate_on_container_copy_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTT::propagate_on_container_copy_assignment::value ));

      //propagate_on_container_move_assignment
      //0 inner
      BOOST_STATIC_ASSERT(( !Scoped0InnerF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped0InnerT::propagate_on_container_move_assignment::value ));
      //1 inner
      BOOST_STATIC_ASSERT(( !Scoped1InnerFF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerFT::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTT::propagate_on_container_move_assignment::value ));
      //2 inner
      BOOST_STATIC_ASSERT(( !Scoped2InnerFFF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFFT::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFTF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFTT::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTFF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTFT::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTF::propagate_on_container_move_assignment::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTT::propagate_on_container_move_assignment::value ));

      //propagate_on_container_swap
      //0 inner
      BOOST_STATIC_ASSERT(( !Scoped0InnerF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped0InnerT::propagate_on_container_swap::value ));
      //1 inner
      BOOST_STATIC_ASSERT(( !Scoped1InnerFF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerFT::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTT::propagate_on_container_swap::value ));
      //2 inner
      BOOST_STATIC_ASSERT(( !Scoped2InnerFFF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFFT::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFTF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerFTT::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTFF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTFT::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTF::propagate_on_container_swap::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTT::propagate_on_container_swap::value ));
      //is_always_equal
      //0 inner
      BOOST_STATIC_ASSERT(( !Scoped0InnerF::is_always_equal::value ));
      BOOST_STATIC_ASSERT((  Scoped0InnerT::is_always_equal::value ));
      //1 inner
      BOOST_STATIC_ASSERT(( !Scoped1InnerFF::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped1InnerFT::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped1InnerTF::is_always_equal::value ));
      BOOST_STATIC_ASSERT((  Scoped1InnerTT::is_always_equal::value ));
      //2 inner
      BOOST_STATIC_ASSERT(( !Scoped2InnerFFF::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped2InnerFFT::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped2InnerFTF::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped2InnerFTT::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped2InnerTFF::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped2InnerTFT::is_always_equal::value ));
      BOOST_STATIC_ASSERT(( !Scoped2InnerTTF::is_always_equal::value ));
      BOOST_STATIC_ASSERT((  Scoped2InnerTTT::is_always_equal::value ));
   }

   //Default constructor
   {
      Scoped0Inner s0i;
      Scoped1Inner s1i;
      //Swap
      {
         Scoped0Inner s0i2;
         Scoped1Inner s1i2;
         boost::adl_move_swap(s0i, s0i2);
         boost::adl_move_swap(s1i, s1i2);
      }
   }

   //Default constructor
   {
      Scoped0Inner s0i;
      Scoped1Inner s1i;
   }

   //Copy constructor/assignment
   {
      Scoped0Inner s0i;
      Scoped1Inner s1i;
      Scoped2Inner s2i;

      Scoped0Inner s0i_b(s0i);
      Scoped1Inner s1i_b(s1i);
      Scoped2Inner s2i_b(s2i);

      BOOST_TEST(s0i == s0i_b);
      BOOST_TEST(s1i == s1i_b);
      BOOST_TEST(s2i == s2i_b);

      s0i_b = s0i;
      s1i_b = s1i;
      s2i_b = s2i;

      BOOST_TEST(s0i == s0i_b);
      BOOST_TEST(s1i == s1i_b);
      BOOST_TEST(s2i == s2i_b);
   }

   //Copy/move constructor/assignment
   {
      Scoped0Inner s0i;
      Scoped1Inner s1i;
      Scoped2Inner s2i;

      Scoped0Inner s0i_b(::boost::move(s0i));
      Scoped1Inner s1i_b(::boost::move(s1i));
      Scoped2Inner s2i_b(::boost::move(s2i));

      BOOST_TEST(s0i_b.outer_allocator().m_move_contructed);
      BOOST_TEST(s1i_b.outer_allocator().m_move_contructed);
      BOOST_TEST(s2i_b.outer_allocator().m_move_contructed);

      s0i_b = ::boost::move(s0i);
      s1i_b = ::boost::move(s1i);
      s2i_b = ::boost::move(s2i);

      BOOST_TEST(s0i_b.outer_allocator().m_move_assigned);
      BOOST_TEST(s1i_b.outer_allocator().m_move_assigned);
      BOOST_TEST(s2i_b.outer_allocator().m_move_assigned);
   }

   //inner_allocator()
   {
      Scoped0Inner s0i;
      Scoped1Inner s1i;
      Scoped2Inner s2i;
      const Scoped0Inner const_s0i;
      const Scoped1Inner const_s1i;
      const Scoped2Inner const_s2i;

      Scoped0Inner::inner_allocator_type &s0i_inner =             s0i.inner_allocator();
      (void)s0i_inner;
      const Scoped0Inner::inner_allocator_type &const_s0i_inner = const_s0i.inner_allocator();
      (void)const_s0i_inner;
      Scoped1Inner::inner_allocator_type &s1i_inner =             s1i.inner_allocator();
      (void)s1i_inner;
      const Scoped1Inner::inner_allocator_type &const_s1i_inner = const_s1i.inner_allocator();
      (void)const_s1i_inner;
      Scoped2Inner::inner_allocator_type &s2i_inner =             s2i.inner_allocator();
      (void)s2i_inner;
      const Scoped2Inner::inner_allocator_type &const_s2i_inner = const_s2i.inner_allocator();
      (void)const_s2i_inner;
   }

   //operator==/!=
   {
      const Scoped0Inner const_s0i;
      const Rebound9Scoped0Inner const_rs0i;

      BOOST_TEST(const_s0i == const_s0i);
      BOOST_TEST(const_rs0i == const_s0i);
      BOOST_TEST(const_s0i == const_s0i);
      BOOST_TEST(const_s0i == const_rs0i);

      const Scoped1Inner const_s1i;
      const Rebound9Scoped1Inner const_rs1i;

      BOOST_TEST(const_s1i == const_s1i);
      BOOST_TEST(const_rs1i == const_s1i);

      BOOST_TEST(const_s1i == const_s1i);
      BOOST_TEST(const_s1i == const_rs1i);

      const Scoped2Inner const_s2i;
      const Rebound9Scoped2Inner const_rs2i;

      BOOST_TEST(const_s2i == const_s2i);
      BOOST_TEST(const_s2i == const_rs2i);

      BOOST_TEST(const_s2i == const_s2i);
      BOOST_TEST(const_s2i == const_rs2i);
   }

   //outer_allocator()
   {
      Scoped0Inner s0i;
      Scoped1Inner s1i;
      Scoped2Inner s2i;
      const Scoped0Inner const_s0i;
      const Scoped1Inner const_s1i;
      const Scoped2Inner const_s2i;

      Scoped0Inner::outer_allocator_type &s0i_inner =             s0i.outer_allocator();
      (void)s0i_inner;
      const Scoped0Inner::outer_allocator_type &const_s0i_inner = const_s0i.outer_allocator();
      (void)const_s0i_inner;
      Scoped1Inner::outer_allocator_type &s1i_inner =             s1i.outer_allocator();
      (void)s1i_inner;
      const Scoped1Inner::outer_allocator_type &const_s1i_inner = const_s1i.outer_allocator();
      (void)const_s1i_inner;
      Scoped2Inner::outer_allocator_type &s2i_inner =             s2i.outer_allocator();
      (void)s2i_inner;
      const Scoped2Inner::outer_allocator_type &const_s2i_inner = const_s2i.outer_allocator();
      (void)const_s2i_inner;
   }

   //max_size()
   {
      const Scoped0Inner const_s0i;
      const Scoped1Inner const_s1i;
      const Scoped2Inner const_s2i;
      const OuterAlloc  const_oa;
      const InnerAlloc1 const_ia1;
      const InnerAlloc2 const_ia2;

      BOOST_TEST(const_s0i.max_size() == const_oa.max_size());
      BOOST_TEST(const_s1i.max_size() == const_oa.max_size());

      BOOST_TEST(const_s2i.max_size() == const_oa.max_size());
      BOOST_TEST(const_s1i.inner_allocator().max_size() == const_ia1.max_size());
      BOOST_TEST(const_s2i.inner_allocator().inner_allocator().max_size() == const_ia2.max_size());
   }
   //Copy and move operations
   {
      //Construction
      {
         Scoped0Inner s0i_a, s0i_b(s0i_a), s0i_c(::boost::move(s0i_b));
         Scoped1Inner s1i_a, s1i_b(s1i_a), s1i_c(::boost::move(s1i_b));
         Scoped2Inner s2i_a, s2i_b(s2i_a), s2i_c(::boost::move(s2i_b));
      }
      //Assignment
      {
         Scoped0Inner s0i_a, s0i_b;
         s0i_a = s0i_b;
         s0i_a = ::boost::move(s0i_b);
         Scoped1Inner s1i_a, s1i_b;
         s1i_a = s1i_b;
         s1i_a = ::boost::move(s1i_b);
         Scoped2Inner s2i_a, s2i_b;
         s2i_a = s2i_b;
         s2i_a = ::boost::move(s2i_b);
      }

      OuterAlloc  oa;
      InnerAlloc1 ia1;
      InnerAlloc2 ia2;
      Rebound9OuterAlloc roa;
      Rebound9Scoped0Inner rs0i;
      Rebound9Scoped1Inner rs1i;
      Rebound9Scoped2Inner rs2i;

      //Copy from outer
      {
         Scoped0Inner s0i(oa);
         Scoped1Inner s1i(oa, ia1);
         Scoped2Inner s2i(oa, ia1, ia2);
      }
      //Move from outer
      {
         Scoped0Inner s0i(::boost::move(oa));
         Scoped1Inner s1i(::boost::move(oa), ia1);
         Scoped2Inner s2i(::boost::move(oa), ia1, ia2);
      }
      //Copy from rebound outer
      {
         Scoped0Inner s0i(roa);
         Scoped1Inner s1i(roa, ia1);
         Scoped2Inner s2i(roa, ia1, ia2);
      }
      //Move from rebound outer
      {
         Scoped0Inner s0i(::boost::move(roa));
         Scoped1Inner s1i(::boost::move(roa), ia1);
         Scoped2Inner s2i(::boost::move(roa), ia1, ia2);
      }
      //Copy from rebound scoped
      {
         Scoped0Inner s0i(rs0i);
         Scoped1Inner s1i(rs1i);
         Scoped2Inner s2i(rs2i);
      }
      //Move from rebound scoped
      {
         Scoped0Inner s0i(::boost::move(rs0i));
         Scoped1Inner s1i(::boost::move(rs1i));
         Scoped2Inner s2i(::boost::move(rs2i));
      }
   }

   {
      vector<int, scoped_allocator_adaptor< propagation_test_allocator<int, 0> > > dummy;
      dummy.push_back(0);
   }

   //destroy()
   {
      {
         Scoped0Inner s0i;
         mark_on_destructor mod;
         s0i.destroy(&mod);
         BOOST_TEST(mark_on_destructor::destroyed);
      }

      {
         Scoped1Inner s1i;
         mark_on_destructor mod;
         s1i.destroy(&mod);
         BOOST_TEST(mark_on_destructor::destroyed);
      }
      {
         Scoped2Inner s2i;
         mark_on_destructor mod;
         s2i.destroy(&mod);
         BOOST_TEST(mark_on_destructor::destroyed);
      }
   }

   //construct
   {
      ////////////////////////////////////////////////////////////
      //First check scoped allocator with just OuterAlloc.
      //In this case OuterAlloc (propagation_test_allocator with tag 0) should be
      //used to construct types.
      ////////////////////////////////////////////////////////////
      {
         Scoped0Inner s0i;
         //Check construction with 0 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s0i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 0 );
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s0i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s0i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }

         //Check construction with 1 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s0i.construct(&dummy, 1);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 1);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s0i.construct(&dummy, 2);
            BOOST_TEST(dummy.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.value == 2);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s0i.construct(&dummy, 3);
            BOOST_TEST(dummy.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.value == 3);
            dummy.~MarkType();
         }
      }
      ////////////////////////////////////////////////////////////
      //Then check scoped allocator with OuterAlloc and InnerAlloc.
      //In this case InnerAlloc (propagation_test_allocator with tag 1) should be
      //used to construct types.
      ////////////////////////////////////////////////////////////
      {
         Scoped1Inner s1i;
         //Check construction with 0 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 1> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s1i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 1> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s1i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 1> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s1i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }

         //Check construction with 1 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 1> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s1i.construct(&dummy, 1);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 1);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 1> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s1i.construct(&dummy, 2);
            BOOST_TEST(dummy.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.value == 2);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 1> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            s1i.construct(&dummy, 3);
            BOOST_TEST(dummy.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.value == 3);
            dummy.~MarkType();
         }
      }

      //////////////////////////////////////////////////////////////////////////////////
      //Now test recursive OuterAllocator types (OuterAllocator is a scoped_allocator)
      //////////////////////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////
      //First check scoped allocator with just OuterAlloc.
      //In this case OuterAlloc (propagation_test_allocator with tag 0) should be
      //used to construct types.
      ////////////////////////////////////////////////////////////
      {
         //Check outer_allocator_type is scoped
         BOOST_STATIC_ASSERT(( is_scoped_allocator
            <ScopedScoped0Inner::outer_allocator_type>::value ));
         BOOST_STATIC_ASSERT(( dtl::is_same
            < outermost_allocator<ScopedScoped0Inner>::type
            , Outer10IdAlloc
            >::value ));
         BOOST_STATIC_ASSERT(( dtl::is_same
            < ScopedScoped0Inner::outer_allocator_type
            , scoped_allocator_adaptor<Outer10IdAlloc>
            >::value ));
         BOOST_STATIC_ASSERT(( dtl::is_same
            < scoped_allocator_adaptor<Outer10IdAlloc>::outer_allocator_type
            , Outer10IdAlloc
            >::value ));
         ScopedScoped0Inner ssro0i;
         Outer10IdAlloc & val = outermost_allocator<ScopedScoped0Inner>::get(ssro0i);
         (void)val;
         //Check construction with 0 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro0i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro0i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro0i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }

         //Check construction with 1 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro0i.construct(&dummy, 1);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 1);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro0i.construct(&dummy, 2);
            BOOST_TEST(dummy.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.value == 2);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro0i.construct(&dummy, 3);
            BOOST_TEST(dummy.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.value == 3);
            dummy.~MarkType();
         }
      }
      ////////////////////////////////////////////////////////////
      //Then check scoped allocator with OuterAlloc and InnerAlloc.
      //In this case inner_allocator_type is not convertible to
      //::allocator_argument_tester<XXX, 10> so uses_allocator
      //should be false on all tests.
      ////////////////////////////////////////////////////////////
      {
         //Check outer_allocator_type is scoped
         BOOST_STATIC_ASSERT(( is_scoped_allocator
            <ScopedScoped1Inner::outer_allocator_type>::value ));
         BOOST_STATIC_ASSERT(( dtl::is_same
            < outermost_allocator<ScopedScoped1Inner>::type
            , Outer10IdAlloc
            >::value ));
         BOOST_STATIC_ASSERT(( dtl::is_same
            < ScopedScoped1Inner::outer_allocator_type
            , scoped_allocator_adaptor<Outer10IdAlloc, Inner11IdAlloc1>
            >::value ));
         BOOST_STATIC_ASSERT(( dtl::is_same
            < scoped_allocator_adaptor<Outer10IdAlloc, Inner11IdAlloc1>::outer_allocator_type
            , Outer10IdAlloc
            >::value ));
         BOOST_STATIC_ASSERT(( !
            uses_allocator
               < ::allocator_argument_tester<ConstructibleSuffix, 10>
               , ScopedScoped1Inner::inner_allocator_type::outer_allocator_type
               >::value ));
         ScopedScoped1Inner ssro1i;
         Outer10IdAlloc & val = outermost_allocator<ScopedScoped1Inner>::get(ssro1i);
         (void)val;

         //Check construction with 0 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro1i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro1i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro1i.construct(&dummy);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 0);
            dummy.~MarkType();
         }

         //Check construction with 1 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro1i.construct(&dummy, 1);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 1);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro1i.construct(&dummy, 2);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 2);
            dummy.~MarkType();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 10> MarkType;
            MarkType dummy;
            dummy.~MarkType();
            ssro1i.construct(&dummy, 3);
            BOOST_TEST(dummy.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.value == 3);
            dummy.~MarkType();
         }
      }

      ////////////////////////////////////////////////////////////
      //Now check propagation to pair
      ////////////////////////////////////////////////////////////
      //First check scoped allocator with just OuterAlloc.
      //In this case OuterAlloc (propagation_test_allocator with tag 0) should be
      //used to construct types.
      ////////////////////////////////////////////////////////////
      {
         using dtl::pair;
         typedef propagation_test_allocator< pair< tagged_integer<0>
                               , tagged_integer<0> >, 0> OuterPairAlloc;
         //
         typedef scoped_allocator_adaptor < OuterPairAlloc  >  ScopedPair0Inner;

         ScopedPair0Inner s0i;
         //Check construction with 0 user arguments
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy);
            BOOST_TEST(dummy.first.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy);
            BOOST_TEST(dummy.first.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy);
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         #if defined(BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE)
         //Check construction with 0 user arguments and Std tuple
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         #endif   //BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE
         //Check construction with 1 user arguments for each pair
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, 1, 1);
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, 1, 1);
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, 2, 2);
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         //Check construction with 1 user arguments for each pair and Boost tuple
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, boost::tuple<int>(1), boost::tuple<int>(1));
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, boost::tuple<int>(1), boost::tuple<int>(1));
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, boost::tuple<int>(2), boost::tuple<int>(2));
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         #if defined(BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE)
         //Check construction with 1 user arguments for each pair and Boost tuple
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<int>(1), std::tuple<int>(1));
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<int>(1), std::tuple<int>(1));
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<int>(2), std::tuple<int>(2));
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         #endif   //BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE
         //Check construction with pair copy construction
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy, dummy2;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, dummy2);
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy, dummy2(1, 1);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, dummy2);
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy, dummy2(2, 2);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, dummy2);
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         //Check construction with pair move construction
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy, dummy2(3, 3);
            dummy2.first.construction_type = dummy2.second.construction_type = ConstructibleSuffix;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, ::boost::move(dummy2));
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 3);
            BOOST_TEST(dummy.second.value == 3);
            BOOST_TEST(dummy2.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy2.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy2.first.value  == 0);
            BOOST_TEST(dummy2.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy, dummy2(1, 1);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, ::boost::move(dummy2));
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            BOOST_TEST(dummy2.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy2.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy2.first.value  == 0);
            BOOST_TEST(dummy2.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy, dummy2(2, 2);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, ::boost::move(dummy2));
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            BOOST_TEST(dummy2.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy2.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy2.first.value  == 0);
            BOOST_TEST(dummy2.second.value == 0);
            dummy.~MarkTypePair();
         }
         //Check construction with related pair copy construction
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            pair<int, int> dummy2;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, dummy2);
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            pair<int, int> dummy2(1, 1);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, dummy2);
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            pair<int, int> dummy2(2, 2);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, dummy2);
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         //Check construction with related pair move construction
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            pair<int, int> dummy2(3, 3);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, ::boost::move(dummy2));
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 3);
            BOOST_TEST(dummy.second.value == 3);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            pair<int, int> dummy2(1, 1);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, ::boost::move(dummy2));
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            pair<int, int> dummy2(2, 2);
            dummy.~MarkTypePair();
            s0i.construct(&dummy, ::boost::move(dummy2));
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 2);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         //Check construction with 0/1 arguments for each pair and Boost tuple
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, boost::tuple<>(), boost::tuple<int>(1));
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, boost::tuple<int>(1), boost::tuple<>());
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, boost::tuple<>(), boost::tuple<int>(2));
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         #if defined(BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE)
         //Check construction with 0/1 arguments for each pair and Boost tuple
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<int>(1));
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<int>(1), std::tuple<>());
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 1);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, piecewise_construct, std::tuple<>(), std::tuple<int>(2));
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 0);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
         #endif   //BOOST_CONTAINER_PAIR_TEST_HAS_HEADER_TUPLE

         //Check construction with try_emplace_t 0/1 arguments for each pair
         {
            typedef ::allocator_argument_tester<NotUsesAllocator, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, try_emplace_t(), 5, 1);
            BOOST_TEST(dummy.first.construction_type  == NotUsesAllocator);
            BOOST_TEST(dummy.second.construction_type == NotUsesAllocator);
            BOOST_TEST(dummy.first.value  == 5);
            BOOST_TEST(dummy.second.value == 1);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructibleSuffix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, try_emplace_t(), 6);
            BOOST_TEST(dummy.first.construction_type  == ConstructibleSuffix);
            BOOST_TEST(dummy.second.construction_type == ConstructibleSuffix);
            BOOST_TEST(dummy.first.value  == 6);
            BOOST_TEST(dummy.second.value == 0);
            dummy.~MarkTypePair();
         }
         {
            typedef ::allocator_argument_tester<ConstructiblePrefix, 0> MarkType;
            typedef pair<MarkType, MarkType> MarkTypePair;
            MarkTypePair dummy;
            dummy.~MarkTypePair();
            s0i.construct(&dummy, try_emplace_t(), 7, 2);
            BOOST_TEST(dummy.first.construction_type  == ConstructiblePrefix);
            BOOST_TEST(dummy.second.construction_type == ConstructiblePrefix);
            BOOST_TEST(dummy.first.value  == 7);
            BOOST_TEST(dummy.second.value == 2);
            dummy.~MarkTypePair();
         }
      }
   }

   return ::boost::report_errors();
}
#include <boost/container/detail/config_end.hpp>
