//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/pmr/flat_map.hpp>
#include <boost/static_assert.hpp>
#include <boost/container/detail/type_traits.hpp>

int main()
{
   using namespace boost::container;
   using boost::container::container_detail::is_same;

   typedef flat_map<int, float, std::less<int>, pmr::polymorphic_allocator<std::pair<int, float> > > intcontainer_t;
   BOOST_STATIC_ASSERT(( is_same<intcontainer_t, pmr::flat_map_of<int, float>::type >::value ));
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      BOOST_STATIC_ASSERT(( is_same<intcontainer_t, pmr::flat_map<int, float> >::value ));
   #endif
   return 0;
}
