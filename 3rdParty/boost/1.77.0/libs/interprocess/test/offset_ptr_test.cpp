//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/detail/type_traits.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::interprocess;

class Base
{};

class Derived
   : public Base
{};

class VirtualDerived
   : public virtual Base
{};

void test_types_and_conversions()
{
   typedef offset_ptr<int>                pint_t;
   typedef offset_ptr<const int>          pcint_t;
   typedef offset_ptr<volatile int>       pvint_t;
   typedef offset_ptr<const volatile int> pcvint_t;

   BOOST_STATIC_ASSERT((ipcdetail::is_same<pint_t::element_type, int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<pcint_t::element_type, const int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<pvint_t::element_type, volatile int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<pcvint_t::element_type, const volatile int>::value));

   BOOST_STATIC_ASSERT((ipcdetail::is_same<pint_t::value_type,   int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<pcint_t::value_type,  int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<pvint_t::value_type,  int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<pcvint_t::value_type, int>::value));
   int dummy_int = 9;

   {  pint_t pint(&dummy_int);
      pcint_t  pcint(pint);
      BOOST_TEST(pcint.get() == &dummy_int);
   }
   {  pint_t pint(&dummy_int);
      pvint_t  pvint(pint);
      BOOST_TEST(pvint.get() == &dummy_int);
   }
   {  pint_t pint(&dummy_int);
      pcvint_t  pcvint(pint);
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pcint_t pcint(&dummy_int);
      pcvint_t  pcvint(pcint);
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pvint_t pvint(&dummy_int);
      pcvint_t  pcvint(pvint);
      BOOST_TEST(pcvint.get() == &dummy_int);
   }

   pint_t   pint(0);
   pcint_t  pcint(0);
   pvint_t  pvint(0);
   pcvint_t pcvint(0);

   pint     = &dummy_int;
   pcint    = &dummy_int;
   pvint    = &dummy_int;
   pcvint   = &dummy_int;

   {  pcint  = pint;
      BOOST_TEST(pcint.get() == &dummy_int);
   }
   {  pvint  = pint;
      BOOST_TEST(pvint.get() == &dummy_int);
   }
   {  pcvint = pint;
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pcvint = pcint;
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pcvint = pvint;
      BOOST_TEST(pcvint.get() == &dummy_int);
   }

   BOOST_TEST(pint);

   pint = 0;
   BOOST_TEST(!pint);

   BOOST_TEST(pint == 0);

   BOOST_TEST(0 == pint);

   pint = &dummy_int;
   BOOST_TEST(0 != pint);

   pcint = &dummy_int;

   BOOST_TEST( (pcint - pint) == 0);
   BOOST_TEST( (pint - pcint) == 0);
}

template<class BasePtr, class DerivedPtr>
void test_base_derived_impl()
{
   typename DerivedPtr::element_type d;
   DerivedPtr pderi(&d);

   BasePtr pbase(pderi);
   pbase = pderi;
   BOOST_TEST(pbase == pderi);
   BOOST_TEST(!(pbase != pderi));
   BOOST_TEST((pbase - pderi) == 0);
   BOOST_TEST(!(pbase < pderi));
   BOOST_TEST(!(pbase > pderi));
   BOOST_TEST(pbase <= pderi);
   BOOST_TEST((pbase >= pderi));
}

void test_base_derived()
{
   typedef offset_ptr<Base>               pbase_t;
   typedef offset_ptr<const Base>         pcbas_t;
   typedef offset_ptr<Derived>            pderi_t;
   typedef offset_ptr<VirtualDerived>     pvder_t;

   test_base_derived_impl<pbase_t, pderi_t>();
   test_base_derived_impl<pbase_t, pvder_t>();
   test_base_derived_impl<pcbas_t, pderi_t>();
   test_base_derived_impl<pcbas_t, pvder_t>();
}

void test_arithmetic()
{
   typedef offset_ptr<int> pint_t;
   const int NumValues = 5;
   int values[NumValues];

   //Initialize p
   pint_t p = values;
   BOOST_TEST(p.get() == values);

   //Initialize p + NumValues
   pint_t pe = &values[NumValues];
   BOOST_TEST(pe != p);
   BOOST_TEST(pe.get() == &values[NumValues]);

   //ptr - ptr
   BOOST_TEST((pe - p) == NumValues);

   //ptr - integer
   BOOST_TEST((pe - NumValues) == p);

   //ptr + integer
   BOOST_TEST((p + NumValues) == pe);

   //integer + ptr
   BOOST_TEST((NumValues + p) == pe);

   //indexing
   BOOST_TEST(pint_t(&p[NumValues]) == pe);
   BOOST_TEST(pint_t(&pe[-NumValues]) == p);

   //ptr -= integer
   pint_t p0 = pe;
   p0-= NumValues;
   BOOST_TEST(p == p0);

   //ptr += integer
   pint_t penew = p0;
   penew += NumValues;
   BOOST_TEST(penew == pe);

   //++ptr
   penew = p0;
   for(int j = 0; j != NumValues; ++j, ++penew);
   BOOST_TEST(penew == pe);

   //--ptr
   p0 = pe;
   for(int j = 0; j != NumValues; ++j, --p0);
   BOOST_TEST(p == p0);

   //ptr++
   penew = p0;
   for(int j = 0; j != NumValues; ++j){
      pint_t p_new_copy = penew;
      BOOST_TEST(p_new_copy == penew++);
   }
   //ptr--
   p0 = pe;
   for(int j = 0; j != NumValues; ++j){
      pint_t p0_copy = p0;
      BOOST_TEST(p0_copy == p0--);
   }
}

void test_comparison()
{
   typedef offset_ptr<int> pint_t;
   const int NumValues = 5;
   int values[NumValues];

   //Initialize p
   pint_t p = values;
   BOOST_TEST(p.get() == values);

   //Initialize p + NumValues
   pint_t pe = &values[NumValues];
   BOOST_TEST(pe != p);

   BOOST_TEST(pe.get() == &values[NumValues]);

   //operators
   BOOST_TEST(p != pe);
   BOOST_TEST(p == p);
   BOOST_TEST((p < pe));
   BOOST_TEST((p <= pe));
   BOOST_TEST((pe > p));
   BOOST_TEST((pe >= p));
}

bool test_pointer_traits()
{
   typedef offset_ptr<int> OInt;
   typedef boost::intrusive::pointer_traits< OInt > PTOInt;
   BOOST_STATIC_ASSERT((ipcdetail::is_same<PTOInt::element_type, int>::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<PTOInt::pointer, OInt >::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<PTOInt::difference_type, OInt::difference_type >::value));
   BOOST_STATIC_ASSERT((ipcdetail::is_same<PTOInt::rebind_pointer<double>::type, offset_ptr<double> >::value));
   int dummy;
   OInt oi(&dummy);
   if(boost::intrusive::pointer_traits<OInt>::pointer_to(dummy) != oi){
      return false;
   }
   return true;
}

struct node 
{
   offset_ptr<node> next;
};

void test_pointer_plus_bits()
{
   BOOST_STATIC_ASSERT((boost::intrusive::max_pointer_plus_bits< offset_ptr<void>, boost::move_detail::alignment_of<node>::value >::value >= 1U));
   typedef boost::intrusive::pointer_plus_bits< offset_ptr<node>, 1u > ptr_plus_bits;

   node n, n2;
   offset_ptr<node> pnode(&n);

   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n);
   BOOST_TEST(0 == ptr_plus_bits::get_bits(pnode));
   ptr_plus_bits::set_bits(pnode, 1u);
   BOOST_TEST(1 == ptr_plus_bits::get_bits(pnode));
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n);

   ptr_plus_bits::set_pointer(pnode, &n2);
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n2);
   BOOST_TEST(1 == ptr_plus_bits::get_bits(pnode));
   ptr_plus_bits::set_bits(pnode, 0u);
   BOOST_TEST(0 == ptr_plus_bits::get_bits(pnode));
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n2);

   ptr_plus_bits::set_pointer(pnode, offset_ptr<node>());
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) ==0);
   BOOST_TEST(0 == ptr_plus_bits::get_bits(pnode));
   ptr_plus_bits::set_bits(pnode, 1u);
   BOOST_TEST(1 == ptr_plus_bits::get_bits(pnode));
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == 0);
}

int main()
{
   test_types_and_conversions();
   test_base_derived();
   test_arithmetic();
   test_comparison();
   test_pointer_traits();
   test_pointer_plus_bits();
   return ::boost::report_errors();
}
