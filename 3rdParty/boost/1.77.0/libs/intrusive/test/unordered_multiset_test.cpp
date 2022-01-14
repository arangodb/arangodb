/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2015.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#include <boost/intrusive/unordered_set.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/intrusive/detail/iterator.hpp>
#include "itestvalue.hpp"
#include "smart_ptr.hpp"
#include "common_functors.hpp"
#include <vector>
#include <algorithm> //std::sort
#include <set>
#include <boost/core/lightweight_test.hpp>

#include "test_macros.hpp"
#include "test_container.hpp"
#include "unordered_test_common.hpp"
#include "unordered_test.hpp"

using namespace boost::intrusive;

template < class ValueTraits, bool ConstantTimeSize, bool CacheBegin, bool CompareHash, bool Incremental, bool Map, bool DefaultHolder >
struct rebinder
{
   typedef unordered_rebinder_common<ValueTraits, DefaultHolder, Map> common_t;
   typedef typename ValueContainer< typename ValueTraits::value_type >::type value_cont_type;

   template < class Option1 =void
            , class Option2 =void
            >
   struct container
   {
      typedef unordered_multiset
         < typename common_t::value_type
         , value_traits<ValueTraits>
         , constant_time_size<ConstantTimeSize>
         , cache_begin<CacheBegin>
         , compare_hash<CompareHash>
         , incremental<Incremental>
         , typename common_t::holder_opt
         , typename common_t::key_of_value_opt
         , Option1
         , Option2
         > type;
      BOOST_STATIC_ASSERT((key_type_tester<typename common_t::key_of_value_opt, type>::value));
      BOOST_STATIC_ASSERT((boost::intrusive::test::is_multikey_true<type>::value));
   };
};

enum HookType
{
   Base,
   Member,
   NonMember
};

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map, HookType Type>
class test_main_template;

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, ConstantTimeSize, DefaultHolder, Map, Base>
{
   public:
   static void execute()
   {
      typedef testvalue<unordered_hooks<VoidPointer> > value_type;
      static const int random_init[6] = { 3, 2, 4, 1, 5, 2 };
      typedef typename ValueContainer< value_type >::type value_cont_type;
      value_cont_type data (6);
      for (int i = 0; i < 6; ++i)
         data[i].value_ = random_init[i];

      typedef testvalue_traits< unordered_hooks<VoidPointer> > testval_traits_t;
      //base
      typedef typename detail::if_c
         < ConstantTimeSize
         , typename testval_traits_t::base_value_traits
         , typename testval_traits_t::auto_base_value_traits   //store_hash<true>
         >::type base_hook_t;
      test::test_unordered
         <  //cache_begin, compare_hash, incremental
            rebinder<base_hook_t, ConstantTimeSize, ConstantTimeSize, !ConstantTimeSize, !!ConstantTimeSize, Map, DefaultHolder>
         >::test_all(data);
   }
};

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, ConstantTimeSize, DefaultHolder, Map, Member>
{
   public:
   static void execute()
   {
      typedef testvalue<unordered_hooks<VoidPointer> > value_type;
      static const int random_init[6] = { 3, 2, 4, 1, 5, 2 };
      typedef typename ValueContainer< value_type >::type value_cont_type;
      value_cont_type data (6);
      for (int i = 0; i < 6; ++i)
         data[i].value_ = random_init[i];

      typedef testvalue_traits< unordered_hooks<VoidPointer> > testval_traits_t;
      //member
      typedef typename detail::if_c
         < ConstantTimeSize
         , typename testval_traits_t::member_value_traits      //optimize_multikey<true>
         , typename testval_traits_t::auto_member_value_traits //store_hash<true>, optimize_multikey<true>
         >::type member_hook_t;
      test::test_unordered
         < //cache_begin, compare_hash, incremental
           rebinder<member_hook_t, ConstantTimeSize, false, !ConstantTimeSize, false, !ConstantTimeSize, DefaultHolder>
         >::test_all(data);
   }
};

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, ConstantTimeSize, DefaultHolder, Map, NonMember>

{
   public:
   static void execute()
   {
      typedef testvalue<unordered_hooks<VoidPointer> > value_type;
      static const int random_init[6] = { 3, 2, 4, 1, 5, 2 };
      typedef typename ValueContainer< value_type >::type value_cont_type;
      value_cont_type data (6);
      for (int i = 0; i < 6; ++i)
         data[i].value_ = random_init[i];

      typedef testvalue_traits< unordered_hooks<VoidPointer> > testval_traits_t;
      //nonmember
      test::test_unordered
         < //cache_begin, compare_hash, incremental
           rebinder<typename testval_traits_t::nonhook_value_traits, ConstantTimeSize, false, false, false, Map, DefaultHolder>
         >::test_all(data);
   }
};

int main()
{
   //VoidPointer x ConstantTimeSize x Map x DefaultHolder

   //void pointer
   test_main_template<void*, false, false, false, Base>::execute();
   test_main_template<void*, false,  true, false, Member>::execute();
   test_main_template<void*,  true, false, false, NonMember>::execute();
   test_main_template<void*,  true,  true, false, Base>::execute();

   //smart_ptr
   test_main_template<smart_ptr<void>, false, false, false, Member>::execute();
   test_main_template<smart_ptr<void>, false,  true, false, NonMember>::execute();
   test_main_template<smart_ptr<void>,  true, false, false, Base>::execute();
   test_main_template<smart_ptr<void>,  true,  true, false, Member>::execute();

   ////bounded_ptr (bool ConstantTimeSize, bool Map)
   //test_main_template_bptr< false, false >::execute();
   //test_main_template_bptr< false,  true >::execute();
   //test_main_template_bptr<  true, false >::execute();
   //test_main_template_bptr<  true,  true >::execute();
   return boost::report_errors();
}
