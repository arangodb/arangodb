/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_construction.hpp"

#include <algorithm>
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/type_erasure/any_cast.hpp>
#include <boost/type_erasure/relaxed.hpp>
#include <scoped_allocator>
#include <utility>
#include <vector>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_construction()
{
  {
    PolyCollection        p;
    const PolyCollection& cp=p;
    ValueFactory          v;

    fill<
      constraints<is_equality_comparable,is_copy_constructible>,
      Types...
    >(p,v,2);

    {
      PolyCollection p2{cp};
      BOOST_TEST(p2==p);
    }
    {
      PolyCollection p2;
      p2=cp;
      BOOST_TEST(p2==p);
    }
    {
      PolyCollection p2{cp};
      auto           d2=get_layout_data<Types...>(p2);
      PolyCollection p3{std::move(p2)};
      auto           d3=get_layout_data<Types...>(p3);
      BOOST_TEST(d2==d3);
      BOOST_TEST(p2.empty());
      do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
    }
    {
      PolyCollection p2{cp};
      auto           d2=get_layout_data<Types...>(p2);
      PolyCollection p3;
      p3={std::move(p2)};
      auto           d3=get_layout_data<Types...>(p3);
      BOOST_TEST(d2==d3);
      BOOST_TEST(p2.empty());
      do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
    }
    {
      PolyCollection p2{cp.begin(),cp.end()};
      BOOST_TEST(p2==p);
    }
    {
      using type=first_of<
         constraints<is_equality_comparable,is_copy_constructible>,
         Types...>;

      PolyCollection p2{cp.template begin<type>(),cp.template end<type>()};
      BOOST_TEST(
        p2.size()==cp.template size<type>()&&
        std::equal(
          p2.template begin<type>(),p2.template end<type>(),
          cp.template begin<type>()));
    }
  }

  {
    using rooted_poly_collection=
      realloc_poly_collection<PolyCollection,rooted_allocator>;
    using allocator_type=typename rooted_poly_collection::allocator_type;

    allocator_type                root1{0},root2{0};
    rooted_poly_collection        p{root1};
    const rooted_poly_collection& cp=p;
    ValueFactory                  v;

    fill<
      constraints<is_equality_comparable,is_copy_constructible>,
      Types...
    >(p,v,2);

    {
      rooted_poly_collection p2{cp,root2};
      BOOST_TEST(p2==p);
      BOOST_TEST(p2.get_allocator()==root2);
    }
    {
      rooted_poly_collection p2{root2};
      p2=cp;
      BOOST_TEST(p2==p);
      BOOST_TEST(p2.get_allocator().root==&root2);
    }

#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
    /* Limitations from libstdc++-v3 make move construction with allocator
     * decay to copy construction with allocator.
     */
#else
    {
      rooted_poly_collection p2{cp};
      auto                   d2=get_layout_data<Types...>(p2);
      rooted_poly_collection p3{std::move(p2),root2};
      auto                   d3=get_layout_data<Types...>(p3);
      BOOST_TEST(d2==d3);
      BOOST_TEST(p2.empty());
      do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
      BOOST_TEST(p3.get_allocator().root==&root2);
    }
#endif
    {
      rooted_poly_collection p2{cp};
      auto                   d2=get_layout_data<Types...>(p2);
      rooted_poly_collection p3{root2};
      p3=std::move(p2);
      auto                   d3=get_layout_data<Types...>(p3);
      BOOST_TEST(d2==d3);
      BOOST_TEST(p2.empty());
      do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)||\
    BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
#else
      BOOST_TEST(p3.get_allocator().root==&root1);
#endif
    }
  }

  {
    using not_copy_constructible=
      boost::poly_collection::not_copy_constructible;

    PolyCollection        p;
    const PolyCollection& cp=p;
    ValueFactory          v;

    fill<
      constraints<is_equality_comparable,is_not_copy_constructible>,
      Types...
    >(p,v,2);

#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
    /* std::unordered_map copy construction and assigment crash when elements
     * throw on copy construction.
     */

    static_assert(
      sizeof(not_copy_constructible)>0,""); /* Wunused-local-typedefs */
    (void)cp;                               /* Wunused-variable       */
#else
    check_throw<not_copy_constructible>([&]{
      PolyCollection p2{cp};
      (void)p2;      
    });
    check_throw<not_copy_constructible>([&]{
      PolyCollection p2;
      p2=cp;
    });
#endif

    {
      PolyCollection p2{std::move(p)};
      BOOST_TEST(!p2.empty());
      BOOST_TEST(p.empty());
      do_((BOOST_TEST(!p.template is_registered<Types>()),0)...);

      p={std::move(p2)};
      BOOST_TEST(!p.empty());
      BOOST_TEST(p2.empty());
      do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
    }
  }

  {
    PolyCollection p1,p2;
    ValueFactory   v;

    fill<constraints<>,Types...>(p1,v,2);
    auto d1=get_layout_data<Types...>(p1),
         d2=get_layout_data<Types...>(p2);

    p1.swap(p2);
    auto e1=get_layout_data<Types...>(p1),
         e2=get_layout_data<Types...>(p2);
    BOOST_TEST(d1==e2);
    BOOST_TEST(d2==e1);
    do_((BOOST_TEST(!p1.template is_registered<Types>()),0)...);

    using std::swap;
    swap(p1,p2);
    auto f1=get_layout_data<Types...>(p1),
         f2=get_layout_data<Types...>(p2);
    BOOST_TEST(e1==f2);
    BOOST_TEST(e2==f1);
    do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
  }
}

void test_scoped_allocator()
{
  using vector_allocator=rooted_allocator<char>;
  using vector=std::vector<char,vector_allocator>;
  using concept_=boost::type_erasure::relaxed;
  using element_allocator=rooted_allocator<
    boost::poly_collection::any_collection_value_type<concept_>
  >;
  using collection_allocator=std::scoped_allocator_adaptor<
    element_allocator,
    vector_allocator
   >;
  using poly_collection=
    boost::any_collection<concept_,collection_allocator>;

  element_allocator    roote{0}; 
  vector_allocator     rootv{0}; 
  collection_allocator al{roote,rootv};
  poly_collection      p{al};

  p.emplace<vector>();
  auto& s=boost::type_erasure::any_cast<vector&>(*p.begin());
  BOOST_TEST(p.get_allocator().root==&roote);

#if BOOST_WORKAROUND(BOOST_MSVC,>=1910)
  /* https://connect.microsoft.com/VisualStudio/feedback/details/3136309 */
#else
  BOOST_TEST(s.get_allocator().root==&rootv);  
#endif
}

void test_construction()
{
  test_construction<
    any_types::collection,auto_increment,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
  test_construction<
    base_types::collection,auto_increment,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
  test_construction<
    function_types::collection,auto_increment,
    function_types::t1,function_types::t2,function_types::t3,
    function_types::t4,function_types::t5>();
  test_scoped_allocator();
}
