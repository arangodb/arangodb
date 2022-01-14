//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/core/lightweight_test.hpp>

#include "derived_from_memory_resource.hpp"
#include "propagation_test_allocator.hpp"

using namespace boost::container::pmr;
using namespace boost::container;

void test_default_constructor()
{
   polymorphic_allocator<int> a;
   BOOST_TEST(a.resource() == get_default_resource());
}

void test_resource_constructor()
{
   derived_from_memory_resource d;
   polymorphic_allocator<int> b(&d);
   BOOST_TEST(&d == b.resource());
}

void test_copy_constructor()
{
   derived_from_memory_resource d;
   polymorphic_allocator<int> b(&d);
   polymorphic_allocator<int> c(b);
   BOOST_TEST(b.resource() == c.resource());
}

void test_copy_assignment()
{
   derived_from_memory_resource d;
   polymorphic_allocator<int> b(&d);
   polymorphic_allocator<int> c;
   BOOST_TEST(c.resource() == get_default_resource());
   c = b;
   BOOST_TEST(c.resource() == b.resource());
}

void test_allocate()
{
   int dummy;
   derived_from_memory_resource d;
   polymorphic_allocator<int> p(&d);
   d.reset();
   d.do_allocate_return = &dummy;
   p.allocate(2);
   BOOST_TEST(d.do_allocate_called == true);
   BOOST_TEST(d.do_allocate_return == &dummy);
   //It shall allocate 2*sizeof(int), alignment_of<int>
   BOOST_TEST(d.do_allocate_bytes == 2*sizeof(int));
   BOOST_TEST(d.do_allocate_alignment == dtl::alignment_of<int>::value);
}

void test_deallocate()
{
   int dummy;
   derived_from_memory_resource d;
   polymorphic_allocator<int> p(&d);
   d.reset();
   p.deallocate(&dummy, 3);
   BOOST_TEST(d.do_deallocate_called == true);
   //It shall deallocate 2*sizeof(int), alignment_of<int>
   BOOST_TEST(d.do_deallocate_p == &dummy);
   BOOST_TEST(d.do_deallocate_bytes == 3*sizeof(int));
   BOOST_TEST(d.do_deallocate_alignment == dtl::alignment_of<int>::value);
}

void test_construct()
{
   //0 arg
   {
      typedef allocator_argument_tester<NotUsesAllocator, 0> value_type;
      value_type value;
      value.~value_type();
      polymorphic_allocator<int> pa;
      pa.construct(&value);
      BOOST_TEST(value.construction_type == NotUsesAllocator);
      BOOST_TEST(value.value == 0);
      value.~value_type();
   }
   {
      typedef allocator_argument_tester<ErasedTypePrefix, 0> value_type;
      value_type value;
      value.~value_type();
      polymorphic_allocator<int> pa;
      pa.construct(&value);
      BOOST_TEST(value.construction_type == ConstructiblePrefix);
      BOOST_TEST(value.value == 0);
      value.~value_type();
   }
   {
      typedef allocator_argument_tester<ErasedTypeSuffix, 0> value_type;
      value_type value;
      value.~value_type();
      polymorphic_allocator<int> pa;
      pa.construct(&value);
      BOOST_TEST(value.construction_type == ConstructibleSuffix);
      BOOST_TEST(value.value == 0);
      value.~value_type();
   }
   //1 arg
   {
      typedef allocator_argument_tester<NotUsesAllocator, 0> value_type;
      value_type value;
      value.~value_type();
      polymorphic_allocator<int> pa;
      pa.construct(&value, 2);
      BOOST_TEST(value.construction_type == NotUsesAllocator);
      BOOST_TEST(value.value == 2);
      value.~value_type();
   }
   {
      typedef allocator_argument_tester<ErasedTypePrefix, 0> value_type;
      value_type value;
      value.~value_type();
      polymorphic_allocator<int> pa;
      pa.construct(&value, 3);
      BOOST_TEST(value.construction_type == ConstructiblePrefix);
      BOOST_TEST(value.value == 3);
      value.~value_type();
   }
   {
      typedef allocator_argument_tester<ErasedTypeSuffix, 0> value_type;
      value_type value;
      value.~value_type();
      polymorphic_allocator<int> pa;
      pa.construct(&value, 4);
      BOOST_TEST(value.construction_type == ConstructibleSuffix);
      BOOST_TEST(value.value == 4);
      value.~value_type();
   }
}

struct char_holder
{
   char m_char;
   ~char_holder()
   {  destructor_called = true;  }
   static bool destructor_called;
};

bool char_holder::destructor_called = false;

void test_destroy()
{
   char_holder ch;
   polymorphic_allocator<int> p;
   BOOST_TEST(char_holder::destructor_called == false);
   p.destroy(&ch);
   BOOST_TEST(char_holder::destructor_called == true);
}

void test_select_on_container_copy_construction()
{
   //select_on_container_copy_construction shall return
   //a default constructed polymorphic_allocator
   //which uses the default resource.
   derived_from_memory_resource d;
   polymorphic_allocator<int> p(&d);
   BOOST_TEST(get_default_resource() == p.select_on_container_copy_construction().resource());
}

void test_resource()
{
   derived_from_memory_resource d;
   polymorphic_allocator<int> p(&d);
   BOOST_TEST(&d == p.resource());
}

int main()
{
   test_default_constructor();
   test_resource_constructor();
   test_copy_constructor();
   test_copy_assignment();
   test_allocate();
   test_deallocate();
   test_construct();
   test_destroy();
   test_select_on_container_copy_construction();
   test_resource();
   return ::boost::report_errors();
}
