/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2015-2015.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_INTRUSIVE_TEST_TEST_COMMON_HPP
#define BOOST_INTRUSIVE_TEST_TEST_COMMON_HPP

#include <boost/intrusive/bs_set_hook.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include "bptr_value.hpp"

namespace boost {
namespace intrusive {

template <class KeyOfValueOption, class Map>
struct key_type_tester
{
   struct empty_default{};
   typedef typename pack_options< empty_default, KeyOfValueOption >::type::key_of_value key_of_value_t;

   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same< KeyOfValueOption
                                                          , key_of_value<int_holder_key_of_value<typename Map::value_type> >
                                                          >::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same< key_of_value_t
                                                          , int_holder_key_of_value<typename Map::value_type>
                                                          >::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same< typename Map::key_type
                                                          , typename key_of_value_t::type >::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same< typename Map::key_of_value
                                                          , key_of_value_t >::value ));
   static const bool value = true;
};

template <class Map>
struct key_type_tester<void, Map>
{
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same< typename Map::key_type
                                                          , typename Map::value_type
                                                          >::value ));

   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same< typename Map::key_of_value
                                                         , boost::intrusive::detail::identity< typename Map::value_type>
                                                         >::value ));
   static const bool value = true;
};

}  //namespace intrusive {
}  //namespace boost {

#endif //BOOST_INTRUSIVE_TEST_TEST_COMMON_HPP
