//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/uses_allocator_fwd.hpp>
#include <boost/container/uses_allocator.hpp>
#include "propagation_test_allocator.hpp"

struct not_uses_allocator
{};

struct uses_allocator_and_not_convertible_to_int
{
   typedef uses_allocator_and_not_convertible_to_int allocator_type;
};

struct uses_allocator_and_convertible_to_int
{
   typedef long allocator_type;
};

struct uses_erased_type_allocator
{
   typedef boost::container::erased_type allocator_type;
};


int main()
{
   using namespace boost::container;
   //Using dummy classes
   BOOST_STATIC_ASSERT(( false == uses_allocator
                           < not_uses_allocator, int>::value ));

   BOOST_STATIC_ASSERT(( false == uses_allocator
                           < uses_allocator_and_not_convertible_to_int, int>::value ));

   BOOST_STATIC_ASSERT((  true == uses_allocator
                           < uses_allocator_and_convertible_to_int, int>::value ));

   BOOST_STATIC_ASSERT((  true == uses_allocator
                           < uses_erased_type_allocator, int>::value ));

   //Using an allocator-like class
   BOOST_STATIC_ASSERT(( false == uses_allocator
                           < allocator_argument_tester<NotUsesAllocator, 0>
                           , propagation_test_allocator<float, 0>
                           >::value ));
   BOOST_STATIC_ASSERT((  true == uses_allocator
                           < allocator_argument_tester<ConstructiblePrefix, 0>
                           , propagation_test_allocator<float, 0>
                           >::value ));
   BOOST_STATIC_ASSERT((  true == uses_allocator
                           < allocator_argument_tester<ConstructibleSuffix, 0>
                           , propagation_test_allocator<float, 0>
                           >::value ));
   BOOST_STATIC_ASSERT((  true == uses_allocator
                           < allocator_argument_tester<ErasedTypeSuffix, 0>
                           , propagation_test_allocator<float, 0>
                           >::value ));
   BOOST_STATIC_ASSERT((  true == uses_allocator
                           < allocator_argument_tester<ErasedTypePrefix, 0>
                           , propagation_test_allocator<float, 0>
                           >::value ));
   BOOST_STATIC_ASSERT((  true == constructible_with_allocator_prefix
                           < allocator_argument_tester<ConstructiblePrefix, 0> >::value ));

   BOOST_STATIC_ASSERT((  true == constructible_with_allocator_suffix
                           < allocator_argument_tester<ConstructibleSuffix, 0> >::value ));

   BOOST_STATIC_ASSERT((  true == constructible_with_allocator_prefix
                           < allocator_argument_tester<ErasedTypePrefix, 0> >::value ));

   BOOST_STATIC_ASSERT((  true == constructible_with_allocator_suffix
                           < allocator_argument_tester<ErasedTypeSuffix, 0> >::value ));
   return 0;
}
