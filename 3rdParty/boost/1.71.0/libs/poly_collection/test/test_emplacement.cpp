/* Copyright 2016 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_emplacement.hpp"

#include <boost/core/lightweight_test.hpp>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_emplacement()
{
  {
    using type=first_of<
      constraints<
        is_constructible_from_int,is_not_copy_constructible,
        is_not_copy_assignable,
        is_equality_comparable
      >,
      Types...
    >;
    using iterator=typename PolyCollection::iterator;
    using local_base_iterator=typename PolyCollection::local_base_iterator;
    using local_iterator=
      typename PolyCollection::template local_iterator<type>;

    PolyCollection p;

    iterator it=p.template emplace<type>(4);
    BOOST_TEST(*p.template begin<type>()==type{4});
    BOOST_TEST(&*it==&*p.begin(typeid(type)));

    iterator it2=p.template emplace_hint<type>(it,3);
    BOOST_TEST(*p.template begin<type>()==type{3});
    BOOST_TEST(&*it2==&*p.begin(typeid(type)));

    iterator it3=p.template emplace_hint<type>(p.cend(),5);
    BOOST_TEST(*(p.template end<type>()-1)==type{5});
    BOOST_TEST(&*it3==&*(p.end(typeid(type))-1));

    local_base_iterator lbit=
      p.template emplace_pos<type>(p.begin(typeid(type)),2);
    BOOST_TEST(*static_cast<local_iterator>(lbit)==type{2});
    BOOST_TEST(lbit==p.begin(typeid(type)));

    local_base_iterator lbit2=
      p.template emplace_pos<type>(p.cend(typeid(type)),6);
    BOOST_TEST(*static_cast<local_iterator>(lbit2)==type{6});
    BOOST_TEST(lbit2==p.end(typeid(type))-1);

    local_iterator lit=p.emplace_pos(p.template begin<type>(),1);
    BOOST_TEST(*lit==type{1});
    BOOST_TEST(lit==p.template begin<type>());

    local_iterator lit2=p.emplace_pos(p.template cend<type>(),7);
    BOOST_TEST(*lit2==type{7});
    BOOST_TEST(lit2==p.template end<type>()-1);
  }
  {
    using type=first_of<
      constraints<is_default_constructible>,
      Types...
    >;

    PolyCollection p;

    p.template emplace<type>();
    p.template emplace_hint<type>(p.begin());
    p.template emplace_hint<type>(p.cend());
    p.template emplace_pos<type>(p.begin(typeid(type)));
    p.template emplace_pos<type>(p.cend(typeid(type)));
    p.emplace_pos(p.template begin<type>());
    p.emplace_pos(p.template cend<type>());
    BOOST_TEST(p.size()==7);
  }
  {
    using type=first_of<
      constraints<is_not_copy_constructible>,
      Types...
    >;

    PolyCollection p;
    ValueFactory   v;

    p.template emplace<type>(v.template make<type>());
    p.template emplace_hint<type>(p.begin(),v.template make<type>());
    p.template emplace_hint<type>(p.cend(),v.template make<type>());
    p.template emplace_pos<type>(
      p.begin(typeid(type)),v.template make<type>());
    p.template emplace_pos<type>(
      p.cend(typeid(type)),v.template make<type>());
    p.emplace_pos(p.template begin<type>(),v.template make<type>());
    p.emplace_pos(p.template cend<type>(),v.template make<type>());
    BOOST_TEST(p.size()==7);
  }
}

void test_emplacement()
{
  test_emplacement<
    any_types::collection,auto_increment,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
  test_emplacement<
    base_types::collection,auto_increment,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
  test_emplacement<
    function_types::collection,auto_increment,
    function_types::t1,function_types::t2,function_types::t3,
    function_types::t4,function_types::t5>();
}
