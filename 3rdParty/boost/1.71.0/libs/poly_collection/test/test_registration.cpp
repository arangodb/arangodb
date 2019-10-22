/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_registration.hpp"

#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename PolyCollection,typename Type>
void test_registration()
{
  using unregistered_type=boost::poly_collection::unregistered_type;

  {
    PolyCollection        p;
    const PolyCollection& cp=p;

    BOOST_TEST(!p.is_registered(typeid(Type)));
    BOOST_TEST(!p.template is_registered<Type>());
    check_throw<unregistered_type>(
      [&]{(void)p.begin(typeid(Type));},
      [&]{(void)p.end(typeid(Type));},
      [&]{(void)cp.begin(typeid(Type));},
      [&]{(void)cp.end(typeid(Type));},
      [&]{(void)p.cbegin(typeid(Type));},
      [&]{(void)p.cend(typeid(Type));},
      [&]{(void)p.template begin<Type>();},
      [&]{(void)p.template end<Type>();},
      [&]{(void)cp.template begin<Type>();},
      [&]{(void)cp.template end<Type>();},
      [&]{(void)p.template cbegin<Type>();},
      [&]{(void)p.template cend<Type>();},
      [&]{(void)p.segment(typeid(Type));},
      [&]{(void)cp.segment(typeid(Type));},
      [&]{(void)p.template segment<Type>();},
      [&]{(void)cp.template segment<Type>();},
      [&]{(void)cp.empty(typeid(Type));},
      [&]{(void)cp.size(typeid(Type));},
      [&]{(void)cp.max_size(typeid(Type));},
      [&]{(void)p.reserve(typeid(Type),0);},
      [&]{(void)cp.capacity(typeid(Type));},
      [&]{(void)p.shrink_to_fit(typeid(Type));},
      [&]{(void)p.clear(typeid(Type));},
      [&]{(void)cp.template empty<Type>();},
      [&]{(void)cp.template size<Type>();},
      [&]{(void)cp.template max_size<Type>();},
      /* reserve<Type> omitted as it actually registers the type */
      [&]{(void)cp.template capacity<Type>();},
      [&]{(void)p.template shrink_to_fit<Type>();},
      [&]{(void)p.template clear<Type>();});

    p.register_types();
    p.template register_types<>();
    BOOST_TEST(!p.is_registered(typeid(Type)));

    p.template register_types<Type>();

    BOOST_TEST(p.is_registered(typeid(Type)));
    BOOST_TEST(p.template is_registered<Type>());
    (void)p.end(typeid(Type));
    (void)cp.begin(typeid(Type));
    (void)cp.end(typeid(Type));
    (void)p.cbegin(typeid(Type));
    (void)p.cend(typeid(Type));
    (void)p.template begin<Type>();
    (void)p.template end<Type>();
    (void)cp.template begin<Type>();
    (void)cp.template end<Type>();
    (void)cp.template cbegin<Type>();
    (void)cp.template cend<Type>();
    (void)cp.empty(typeid(Type));
    (void)cp.size(typeid(Type));
    (void)cp.max_size(typeid(Type));
    (void)p.reserve(typeid(Type),0);
    (void)cp.capacity(typeid(Type));
    (void)p.shrink_to_fit(typeid(Type));
    (void)p.clear(typeid(Type));
    (void)cp.template empty<Type>();
    (void)cp.template size<Type>();
    (void)cp.template max_size<Type>();
    /* reserve<Type> omitted */
    (void)cp.template capacity<Type>();
    (void)p.template shrink_to_fit<Type>();
    (void)p.template clear<Type>();
  }

  {
    PolyCollection p;
    p.template reserve<Type>(0);
    BOOST_TEST(p.is_registered(typeid(Type)));
  }

  {
    PolyCollection p;
    p.template register_types<Type,Type,Type>();
    BOOST_TEST(p.is_registered(typeid(Type)));
    BOOST_TEST(
      std::distance(
        p.segment_traversal().begin(),p.segment_traversal().end())==1);
  }
}

void test_registration()
{
  test_registration<any_types::collection,any_types::t1>();
  test_registration<base_types::collection,base_types::t1>();
  test_registration<function_types::collection,function_types::t1>();
}
