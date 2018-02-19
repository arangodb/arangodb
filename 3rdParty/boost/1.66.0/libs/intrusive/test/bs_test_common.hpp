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
#ifndef BOOST_INTRUSIVE_TEST_BS_TEST_COMMON_HPP
#define BOOST_INTRUSIVE_TEST_BS_TEST_COMMON_HPP

#include <boost/intrusive/bs_set_hook.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include "bptr_value.hpp"
#include "test_common.hpp"

namespace boost {
namespace intrusive {

template<class VoidPointer>
struct bs_hooks
{
   typedef bs_set_base_hook<void_pointer<VoidPointer> >     base_hook_type;
   typedef bs_set_base_hook
      <link_mode<auto_unlink>
      , void_pointer<VoidPointer>
      , tag<void> >                                         auto_base_hook_type;
   typedef bs_set_member_hook
      <void_pointer<VoidPointer> >                          member_hook_type;
   typedef bs_set_member_hook
      < link_mode<auto_unlink>
      , void_pointer<VoidPointer> >                         auto_member_hook_type;
   typedef nonhook_node_member< tree_node_traits<VoidPointer>,
                                bstree_algorithms
                              > nonhook_node_member_type;
};

template < class ValueTraits, bool DefaultHolder, bool Map >
struct tree_rebinder_common
{
   typedef typename ValueTraits::value_type  value_type;
   typedef typename detail::if_c
      < DefaultHolder
      , typename detail::if_c
         < detail::is_same<value_type, BPtr_Value>::value
         , header_holder_type< bounded_pointer_holder< BPtr_Value > >
         , void
         >::type
      , header_holder_type< heap_node_holder< typename ValueTraits::node_ptr > >
      >::type holder_opt;
   typedef typename detail::if_c
      < Map, key_of_value<int_holder_key_of_value<value_type> >, void
      >::type key_of_value_opt;
};


}  //namespace intrusive {
}  //namespace boost {

#endif //BOOST_INTRUSIVE_TEST_BS_TEST_COMMON_HPP
