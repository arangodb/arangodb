//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/pmr/string.hpp>
#include <boost/static_assert.hpp>
#include <boost/container/detail/type_traits.hpp>

int main()
{
   using namespace boost::container;
   using boost::container::dtl::is_same;

   typedef basic_string<char, std::char_traits<char>, pmr::polymorphic_allocator<char> > string_t;
   typedef basic_string<wchar_t, std::char_traits<wchar_t>, pmr::polymorphic_allocator<wchar_t> > wstring_t;
   BOOST_STATIC_ASSERT(( is_same<string_t, pmr::string>::value ));
   BOOST_STATIC_ASSERT(( is_same<string_t, pmr::basic_string_of<char>::type>::value ));
   BOOST_STATIC_ASSERT(( is_same<wstring_t, pmr::wstring>::value ));
   BOOST_STATIC_ASSERT(( is_same<wstring_t, pmr::basic_string_of<wchar_t>::type>::value ));
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      BOOST_STATIC_ASSERT(( is_same<string_t, pmr::string >::value ));
      BOOST_STATIC_ASSERT(( is_same<wstring_t, pmr::wstring >::value ));
   #endif
   return 0;
}
