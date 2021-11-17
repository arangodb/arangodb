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
#include <boost/intrusive/bs_set.hpp>
#include "itestvalue.hpp"
#include "bptr_value.hpp"
#include "smart_ptr.hpp"
#include "generic_set_test.hpp"
#include "bs_test_common.hpp"

using namespace boost::intrusive;

template < class ValueTraits, bool ConstantTimeSize, bool DefaultHolder, bool Map >
struct rebinder
{
   typedef tree_rebinder_common<ValueTraits, DefaultHolder, Map> common_t;
   typedef typename ValueContainer< typename ValueTraits::value_type >::type value_cont_type;

   template < class Option1 =void
            , class Option2 =void
            >
   struct container
   {
      typedef bs_set
         < typename common_t::value_type
         , value_traits<ValueTraits>
         , constant_time_size<ConstantTimeSize>
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

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map, HookType Type>
class test_main_template;

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, ConstantTimeSize, DefaultHolder, Map, Base>
{
   public:
   static void execute()
   {
      typedef testvalue_traits< bs_hooks<VoidPointer> > testval_traits_t;
      //base
      typedef typename detail::if_c
         < ConstantTimeSize
         , typename testval_traits_t::base_value_traits
         , typename testval_traits_t::auto_base_value_traits
         >::type base_hook_t;
      test::test_generic_set
         < rebinder<base_hook_t, ConstantTimeSize, DefaultHolder, Map>
         >::test_all();
   }
};

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, ConstantTimeSize, DefaultHolder, Map, Member>
{
   public:
   static void execute()
   {
      typedef testvalue_traits< bs_hooks<VoidPointer> > testval_traits_t;
      //member
      typedef typename detail::if_c
         < ConstantTimeSize
         , typename testval_traits_t::member_value_traits
         , typename testval_traits_t::auto_member_value_traits
         >::type member_hook_t;
      test::test_generic_set
         < rebinder<member_hook_t, ConstantTimeSize, DefaultHolder, Map>
         >::test_all();
   }
};

template<class VoidPointer, bool ConstantTimeSize, bool DefaultHolder, bool Map>
class test_main_template<VoidPointer, ConstantTimeSize, DefaultHolder, Map, NonMember>
{
   public:
   static void execute()
   {
      typedef testvalue_traits< bs_hooks<VoidPointer> > testval_traits_t;
      //nonmember
      test::test_generic_set
         < rebinder<typename testval_traits_t::nonhook_value_traits, ConstantTimeSize, DefaultHolder, Map>
         >::test_all();
   }
};

template < bool ConstantTimeSize, bool Map >
struct test_main_template_bptr
{
   static void execute()
   {
      typedef BPtr_Value_Traits< Tree_BPtr_Node_Traits > value_traits;
      typedef bounded_allocator< BPtr_Value >            allocator_type;

      bounded_allocator_scope<allocator_type> bounded_scope; (void)bounded_scope;
      test::test_generic_set
         < rebinder< value_traits, ConstantTimeSize, true, Map>
         >::test_all();
   }
};

int main()
{
   //Combinations: VoidPointer x ConstantTimeSize x DefaultHolder x Map
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

   //bounded_ptr (bool ConstantTimeSize, bool Map)
   test_main_template_bptr< false, false >::execute();
   //test_main_template_bptr< false,  true >::execute();
   //test_main_template_bptr<  true, false >::execute();
   test_main_template_bptr<  true,  true >::execute();

   return boost::report_errors();
}
