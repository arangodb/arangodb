//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2019. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/devector.hpp>
#include <boost/container/deque.hpp>
#include <boost/static_assert.hpp>

#include <boost/container/detail/container_or_allocator_rebind.hpp>

#include "map_test.hpp"
#include <map>

using namespace boost::container;

template<class VoidAllocatorOrContainer>
struct GetMapContainer
{
   template<class ValueType>
   struct apply
   {
      typedef std::pair<ValueType, ValueType> type_t;
      typedef flat_map< ValueType
                 , ValueType
                 , std::less<ValueType>
                 , typename boost::container::dtl::container_or_allocator_rebind<VoidAllocatorOrContainer, type_t>::type
                 > map_type;

      typedef flat_multimap< ValueType
                 , ValueType
                 , std::less<ValueType>
                 , typename boost::container::dtl::container_or_allocator_rebind<VoidAllocatorOrContainer, type_t>::type
                 > multimap_type;
   };
};

int main()
{
   using namespace boost::container::test;

   ////////////////////////////////////
   //    Testing sequence container implementations
   ////////////////////////////////////
   {
      typedef std::map<int, int>                                     MyStdMap;
      typedef std::multimap<int, int>                                MyStdMultiMap;

      if (0 != test::map_test
         < GetMapContainer<vector<std::pair<int, int> > >::apply<int>::map_type
         , MyStdMap
         , GetMapContainer<vector<std::pair<int, int> > >::apply<int>::multimap_type
         , MyStdMultiMap>()) {
         std::cout << "Error in map_test<vector<std::pair<int, int> > >" << std::endl;
         return 1;
      }

      if (0 != test::map_test
         < GetMapContainer<small_vector<std::pair<int, int>, 7> >::apply<int>::map_type
         , MyStdMap
         , GetMapContainer<small_vector<std::pair<int, int>, 7> >::apply<int>::multimap_type
         , MyStdMultiMap>()) {
         std::cout << "Error in map_test<small_vector<std::pair<int, int>, 7> >" << std::endl;
         return 1;
      }

       if (0 != test::map_test
         < GetMapContainer<static_vector<std::pair<int, int>, MaxElem * 10> >::apply<int>::map_type
         , MyStdMap
         , GetMapContainer<static_vector<std::pair<int, int>, MaxElem * 10> >::apply<int>::multimap_type
         , MyStdMultiMap>()) {
         std::cout << "Error in map_test<static_vector<std::pair<int, int>, MaxElem * 10> >" << std::endl;
         return 1;
      }

      if (0 != test::map_test
         < GetMapContainer<stable_vector<std::pair<int, int> > >::apply<int>::map_type
         , MyStdMap
         , GetMapContainer<stable_vector<std::pair<int, int> > >::apply<int>::multimap_type
         , MyStdMultiMap>()) {
         std::cout << "Error in map_test<stable_vector<std::pair<int, int> > >" << std::endl;
         return 1;
      }


      if (0 != test::map_test
         < GetMapContainer<deque<std::pair<int, int> > >::apply<int>::map_type
         , MyStdMap
         , GetMapContainer<deque<std::pair<int, int> > >::apply<int>::multimap_type
         , MyStdMultiMap>()) {
         std::cout << "Error in map_test<deque<std::pair<int, int> > >" << std::endl;
         return 1;
      }

      if (0 != test::map_test
         < GetMapContainer<devector<std::pair<int, int> > >::apply<int>::map_type
         , MyStdMap
         , GetMapContainer<devector<std::pair<int, int> > >::apply<int>::multimap_type
         , MyStdMultiMap>()) {
         std::cout << "Error in map_test<vector<std::pair<int, int> > >" << std::endl;
         return 1;
      }
   }
   {
      using namespace boost::container;
      using boost::container::dtl::is_same;

      typedef flat_map<int, float, std::less<int>, small_vector<std::pair<int, float>, 10> > map_container_t;
      typedef flat_multimap<int, float, std::less<int>, small_vector<std::pair<int, float>, 10> > multimap_container_t;
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      BOOST_STATIC_ASSERT(( is_same<map_container_t, small_flat_map<int, float, 10> >::value ));
      BOOST_STATIC_ASSERT(( is_same<multimap_container_t, small_flat_multimap<int, float, 10> >::value ));
      #endif

      BOOST_STATIC_ASSERT(( is_same<map_container_t, small_flat_map_of<int, float, 10>::type >::value ));
      BOOST_STATIC_ASSERT(( is_same<multimap_container_t, small_flat_multimap_of<int, float, 10>::type >::value ));
   }

   return 0;
}
