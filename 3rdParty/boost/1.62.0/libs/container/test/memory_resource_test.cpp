//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include "derived_from_memory_resource.hpp"

#include <cstdlib>

using namespace boost::container;
using namespace boost::container::pmr;

void test_allocate()
{
   derived_from_memory_resource d;
   memory_resource &mr = d;

   d.reset();
   BOOST_TEST(d.do_allocate_called == false);
   BOOST_TEST(d.do_allocate_bytes == 0);
   BOOST_TEST(d.do_allocate_alignment == 0);

   mr.allocate(2, 4);
   BOOST_TEST(d.do_allocate_called == true);
   BOOST_TEST(d.do_allocate_bytes == 2);
   BOOST_TEST(d.do_allocate_alignment == 4);
}

void test_deallocate()
{
   derived_from_memory_resource d;
   memory_resource &mr = d;

   d.reset();
   BOOST_TEST(d.do_deallocate_called == false);
   BOOST_TEST(d.do_deallocate_p == 0);
   BOOST_TEST(d.do_allocate_bytes == 0);
   BOOST_TEST(d.do_allocate_alignment == 0);

   mr.deallocate(&d, 2, 4);
   BOOST_TEST(d.do_deallocate_called == true);
   BOOST_TEST(d.do_deallocate_p == &d);
   BOOST_TEST(d.do_deallocate_bytes == 2);
   BOOST_TEST(d.do_deallocate_alignment == 4);
}

void test_destructor()
{
   {
   derived_from_memory_resource d;
   d.reset();
   BOOST_TEST(derived_from_memory_resource::destructor_called == false);
   }
   BOOST_TEST(derived_from_memory_resource::destructor_called == true);
}

void test_is_equal()
{
   derived_from_memory_resource d;
   memory_resource &mr = d;

   d.reset();
   BOOST_TEST(d.do_is_equal_called == false);
   BOOST_TEST(d.do_is_equal_other == 0);

   mr.is_equal(d);
   BOOST_TEST(d.do_is_equal_called == true);
   BOOST_TEST(d.do_is_equal_other == &d);
}

void test_equality_operator()
{
   derived_from_memory_resource d;
   memory_resource &mr = d;

   d.reset();
   BOOST_TEST(d.do_is_equal_called == false);
   BOOST_TEST(d.do_is_equal_other == 0);

   //equal addresses are shorcircuited
   BOOST_TEST((mr == mr) == true);
   BOOST_TEST(d.do_is_equal_called == false);
   BOOST_TEST(d.do_is_equal_other == 0);

   //unequal addresses are dispatched to is_equal which in turn calls do_is_equal
   derived_from_memory_resource d2(1);
   d.reset();
   d2.reset();
   memory_resource &mr2 = d2;
   BOOST_TEST((mr == mr2) == false);
   BOOST_TEST(d.do_is_equal_called == true);
   BOOST_TEST(d.do_is_equal_other == &d2);
}

void test_inequality_operator()
{
   derived_from_memory_resource d;
   memory_resource &mr = d;

   d.reset();
   BOOST_TEST(d.do_is_equal_called == false);
   BOOST_TEST(d.do_is_equal_other == 0);

   //equal addresses are shorcircuited
   BOOST_TEST((mr != mr) == false);
   BOOST_TEST(d.do_is_equal_called == false);
   BOOST_TEST(d.do_is_equal_other == 0);

   //unequal addresses are dispatched to is_equal which in turn calls do_is_equal
   derived_from_memory_resource d2(1);
   d.reset();
   d2.reset();
   memory_resource &mr2 = d2;
   BOOST_TEST((mr != mr2) == true);
   BOOST_TEST(d.do_is_equal_called == true);
   BOOST_TEST(d.do_is_equal_other == &d2);
}

int main()
{
   test_destructor();
   test_allocate();
   test_deallocate();
   test_is_equal();
   test_equality_operator();
   test_inequality_operator();
   return ::boost::report_errors();
}
