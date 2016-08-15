/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2007-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#include <boost/intrusive/sg_set.hpp>
#include "itestvalue.hpp"
#include "bptr_value.hpp"
#include "smart_ptr.hpp"
#include "bs_test_common.hpp"
#include "generic_set_test.hpp"

using namespace boost::intrusive;

template < class ValueTraits, bool FloatingPoint, bool DefaultHolder, bool Map >
struct rebinder
{
   typedef tree_rebinder_common<ValueTraits, DefaultHolder, Map> common_t;

   template < class Option1 =void
            , class Option2 =void
            >
   struct container
   {
      typedef sg_set
         < typename common_t::value_type
         , value_traits<ValueTraits>
         , floating_point<FloatingPoint>
         , typename common_t::holder_opt
         , typename common_t::key_of_value_opt
         , Option1
         , Option2
         > type;
      BOOST_STATIC_ASSERT((key_type_tester<typename common_t::key_of_value_opt, type>::value));
   };
};

enum HookType
{
   Base,
   Member,
   NonMember
};

template<class VoidPointer, bool FloatingPoint, bool DefaultHolder, bool Map, HookType Type>
class test_main_template;

template<class VoidPointer, bool FloatingPoint, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, FloatingPoint, DefaultHolder, Map, Base>
{
   public:
   static void execute()
   {
      typedef testvalue_traits< bs_hooks<VoidPointer> > testval_traits_t;
      //base
      typedef typename testval_traits_t::base_value_traits  base_hook_t;
      test::test_generic_set
         < base_hook_t
         , rebinder<base_hook_t, FloatingPoint, DefaultHolder, Map>
         >::test_all();
   }
};

template<class VoidPointer, bool FloatingPoint, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, FloatingPoint, DefaultHolder, Map, Member>
{
   public:
   static void execute()
   {
      typedef testvalue_traits< bs_hooks<VoidPointer> > testval_traits_t;
      //member
      typedef typename testval_traits_t::member_value_traits member_hook_t;
      test::test_generic_set
         < member_hook_t
         , rebinder<member_hook_t, FloatingPoint, DefaultHolder, Map>
         >::test_all();
   }
};

template<class VoidPointer, bool FloatingPoint, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, FloatingPoint, DefaultHolder, Map, NonMember>
{
   public:
   static void execute()
   {
      typedef testvalue_traits< bs_hooks<VoidPointer> > testval_traits_t;
      //nonmember
      test::test_generic_set
         < typename testval_traits_t::nonhook_value_traits
         , rebinder<typename testval_traits_t::nonhook_value_traits, FloatingPoint, DefaultHolder, Map>
         >::test_all();
   }
};

template < bool FloatingPoint, bool Map >
struct test_main_template_bptr
{
   static void execute()
   {
      typedef BPtr_Value_Traits< Tree_BPtr_Node_Traits > value_traits;
      typedef bounded_allocator< BPtr_Value >            allocator_type;

      bounded_allocator_scope<allocator_type> bounded_scope; (void)bounded_scope;
      test::test_generic_set
         < value_traits
         , rebinder< value_traits, FloatingPoint, true, Map>
         >::test_all();
   }
};

int main()
{
   //Combinations: VoidPointer x FloatingPoint x DefaultHolder x Map
   //Minimize them selecting different combinations for raw and smart pointers
   //Start with ('false', 'false', 'false') in sets and 'false', 'false', 'true' in multisets

   //void pointer
   test_main_template<void*, false, false, false, Base>::execute();
   //test_main_template<void*, false, false, true>::execute();
   test_main_template<void*, false, true, false, Member>::execute();
   //test_main_template<void*, false, true,  true>::execute();
   test_main_template<void*,  true, false, false, Base>::execute();
   //test_main_template<void*,  true, false, true>::execute();
   test_main_template<void*,  true, true, false, Member>::execute();
   test_main_template<void*,  true, true,  true, NonMember>::execute();

   //smart_ptr
   //test_main_template<smart_ptr<void>, false, false, false>::execute();
   test_main_template<smart_ptr<void>, false, false,  true, Base>::execute();
   //test_main_template<smart_ptr<void>, false,  true, false>::execute();
   test_main_template<smart_ptr<void>, false,  true,  true, Member>::execute();
   //test_main_template<smart_ptr<void>,  true, false, false>::execute();
   test_main_template<smart_ptr<void>,  true, false, true, NonMember>::execute();
   //test_main_template<smart_ptr<void>,  true,  true, false>::execute();
   //test_main_template<smart_ptr<void>,  true,  true,  true>::execute();

   //bounded_ptr (bool FloatingPoint, bool Map)
   test_main_template_bptr< false, false >::execute();
   //test_main_template_bptr< false,  true >::execute();
   //test_main_template_bptr<  true, false >::execute();
   test_main_template_bptr<  true,  true >::execute();

   return boost::report_errors();
}
