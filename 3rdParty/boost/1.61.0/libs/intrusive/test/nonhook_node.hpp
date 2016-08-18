/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Matei David     2014-2014.
// (C) Copyright Ion Gaztanaga   2014-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_INTRUSIVE_DETAIL_NONHOOK_NODE_HPP
#define BOOST_INTRUSIVE_DETAIL_NONHOOK_NODE_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/static_assert.hpp>
#include <boost/intrusive/detail/to_raw_pointer.hpp>
#include <boost/intrusive/detail/parent_from_member.hpp>


namespace boost{
namespace intrusive{

//This node will only be used in safe or auto unlink modes
//so test it's been properly released
template < typename NodeTraits, template <typename> class Node_Algorithms >
struct nonhook_node_member : public NodeTraits::node
{
   typedef NodeTraits                                               node_traits;
   typedef typename node_traits::node                                node;
   typedef typename node_traits::node_ptr                            node_ptr;
   typedef typename node_traits::const_node_ptr                      const_node_ptr;
   typedef Node_Algorithms< node_traits >                            node_algorithms;

   nonhook_node_member()
   {
      node_algorithms::init(pointer_traits<node_ptr>::pointer_to(static_cast< node& >(*this)));
   }

   nonhook_node_member(const nonhook_node_member& rhs)
   {
      BOOST_TEST(!rhs.is_linked());
      *this = rhs;
   }

   nonhook_node_member& operator = (const nonhook_node_member& rhs)
   {
      BOOST_TEST(!this->is_linked() && !rhs.is_linked());
      static_cast< node& >(*this) = rhs;
      return *this;
   }

   ~nonhook_node_member()
   {
      BOOST_TEST(!this->is_linked());
      node_algorithms::init(pointer_traits<node_ptr>::pointer_to(static_cast< node& >(*this)));
   }

   void swap_nodes(nonhook_node_member& other)
   {
      node_algorithms::swap_nodes(pointer_traits<node_ptr>::pointer_to(static_cast< node& >(*this)),
                                  pointer_traits<node_ptr>::pointer_to(static_cast< node& >(other)));
   }

   bool is_linked() const
   {
      return !node_algorithms::unique(pointer_traits<const_node_ptr>::pointer_to(static_cast< const node& >(*this)));
   }
};

template < typename T, typename NonHook_Member, NonHook_Member T::* P, link_mode_type Link_Mode >
struct nonhook_node_member_value_traits
{
   typedef T                                                         value_type;
   typedef typename NonHook_Member::node_traits                      node_traits;
   typedef typename node_traits::node                                node;
   typedef typename node_traits::node_ptr                            node_ptr;
   typedef typename node_traits::const_node_ptr                      const_node_ptr;
   typedef typename pointer_traits<node_ptr>::
      template rebind_pointer<T>::type                               pointer;
   typedef typename pointer_traits<node_ptr>::
      template rebind_pointer<const T>::type                         const_pointer;
   typedef T &                                                       reference;
   typedef const T &                                                 const_reference;

   static const link_mode_type link_mode = Link_Mode;

   BOOST_STATIC_ASSERT((Link_Mode == safe_link || Link_Mode == auto_unlink));

   static node_ptr to_node_ptr(reference value)
   {
      return pointer_traits<node_ptr>::pointer_to(static_cast<node&>(value.*P));
   }

   static const_node_ptr to_node_ptr(const_reference value)
   {
      return pointer_traits<const_node_ptr>::pointer_to(static_cast<const node&>(value.*P));
   }

   static pointer to_value_ptr(node_ptr n)
   {
      return pointer_traits<pointer>::pointer_to
      (*detail::parent_from_member<T, NonHook_Member>
      (static_cast<NonHook_Member*>(boost::intrusive::detail::to_raw_pointer(n)), P));
   }

   static const_pointer to_value_ptr(const_node_ptr n)
   {
      return pointer_traits<const_pointer>::pointer_to
      (*detail::parent_from_member<T, NonHook_Member>
      (static_cast<const NonHook_Member*>(boost::intrusive::detail::to_raw_pointer(n)), P));
   }
};

}
}

#endif
