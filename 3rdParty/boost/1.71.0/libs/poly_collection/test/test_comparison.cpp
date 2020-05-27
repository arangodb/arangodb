/* Copyright 2016-2018 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_comparison.hpp"

#include <boost/core/lightweight_test.hpp>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_comparison()
{
  {
    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;

    BOOST_TEST(cp1==cp1);
    BOOST_TEST(!(cp1!=cp1));
    BOOST_TEST(cp1==cp2);
    BOOST_TEST(!(cp1!=cp2));
  }
  {
    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;
    ValueFactory          v;

    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p1,v,2);

    BOOST_TEST(!(cp1==cp2));
    BOOST_TEST(cp1!=cp2);
  }
  {
    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;
    ValueFactory          v;

    p1.template register_types<Types...>();
    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p1,v,2);

    BOOST_TEST(!(cp1==cp2));
    BOOST_TEST(cp1!=cp2);
  }
  {
    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;
    ValueFactory          v;

    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p1,v,1);
    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p2,v,2);

    BOOST_TEST(!(cp1==cp2));
    BOOST_TEST(cp1!=cp2);
  }
  {
    using not_equality_comparable=
      boost::poly_collection::not_equality_comparable;

    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;
    ValueFactory          v;

    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p1,v,2);
    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p2,v,2);

    check_throw<not_equality_comparable>(
     [&]{(void)(cp1==cp2);},
     [&]{(void)(cp1!=cp2);});
  }
  {
    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;
    ValueFactory          v;

    fill<
      constraints<is_not_equality_comparable>,
      Types...
    >(p1,v,2);
    fill<
      constraints<is_equality_comparable,is_copy_constructible>,
      Types...
    >(p2,v,2);
    p1.insert(p2.begin(),p2.end());

    BOOST_TEST(!(cp1==cp2));
    BOOST_TEST(cp1!=cp2);
  }
  {
    PolyCollection        p1,p2;
    const PolyCollection& cp1=p1;
    const PolyCollection& cp2=p2;
    ValueFactory          v;

    p1.template register_types<Types...>();
    fill<
      constraints<is_equality_comparable,is_copy_constructible>,
      Types...
    >(p2,v,2);
    p1.insert(p2.begin(),p2.end());

    BOOST_TEST(cp1==cp2);
    BOOST_TEST(!(cp1!=cp2));

    p1.erase(p1.begin());
    BOOST_TEST(!(cp1==cp2));
    BOOST_TEST(cp1!=cp2);
  }
}

void test_stateless_lambda_comparability_check()
{
  /* https://svn.boost.org/trac10/ticket/13012 */

  {
    boost::function_collection<void()> c1,c2;
    c1.insert([]{});
    BOOST_TEST(c1!=c2);
  }
  {
    boost::function_collection<int(int)> c1,c2;
    c1.insert([](int){return 0;});
    BOOST_TEST(c1!=c2);
  }
}

void test_comparison()
{
  test_comparison<
    any_types::collection,auto_increment,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
  test_comparison<
    base_types::collection,auto_increment,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
  test_comparison<
    function_types::collection,auto_increment,
    function_types::t1,function_types::t2,function_types::t3,
    function_types::t4,function_types::t5>();
  test_stateless_lambda_comparability_check();
}
