/* Copyright 2016-2020 Joaquin M Lopez Munoz.
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
#include <boost/type_erasure/relaxed.hpp>
#include <scoped_allocator>
#include <utility>
#include <vector>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<
  bool Propagate,bool AlwaysEqual,
  typename PolyCollection,typename ValueFactory,typename... Types
>
void test_allocator_aware_construction()
{
  using rooted_poly_collection=realloc_poly_collection<
    PolyCollection,rooted_allocator,
    std::integral_constant<bool,Propagate>,
    std::integral_constant<bool,AlwaysEqual>>;
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
    rooted_poly_collection p2{cp};
    BOOST_TEST(p2==p);
    BOOST_TEST(p2.get_allocator().comes_from(root1));
  }
  {
    rooted_poly_collection p2{cp};
    auto                   d2=get_layout_data<Types...>(p2);
    rooted_poly_collection p3{std::move(p2)};
    auto                   d3=get_layout_data<Types...>(p3);
    BOOST_TEST(p3==p);
    BOOST_TEST(d2==d3);
    BOOST_TEST(p2.empty());
    do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
    BOOST_TEST(p2.get_allocator().comes_from(root1));
  }
  {
    rooted_poly_collection p2{cp,root2};
    BOOST_TEST(p2==p);
    BOOST_TEST(p2.get_allocator().comes_from(root2));
  }
#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
  /* std::unordered_map allocator move ctor does not work when source and
   * and target allocators are not equal.
   */

  if(AlwaysEqual)
#endif
#if BOOST_WORKAROUND(_MSVC_STL_UPDATE,==201811L)
  /* This particular version of VS2019 has a bug in std::unordered_map
   * allocator move ctor when source and target allocators are not equal.
   * After private communication from Billy O'Neal.
   */

  if(AlwaysEqual)
#endif
  {
    rooted_poly_collection p2{cp};
    auto                   d2=get_layout_data<Types...>(p2);
    rooted_poly_collection p3{std::move(p2),root2};
    auto                   d3=get_layout_data<Types...>(p3);

    BOOST_TEST(p3==p);
    if(AlwaysEqual)BOOST_TEST(d2==d3);

    BOOST_TEST(p2.empty());
    do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);

#if !defined(BOOST_MSVC)&&\
    BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB,BOOST_TESTED_AT(804))
    /* Very odd behavior probably due to std::unordered_map allocator move
     * ctor being implemented with move assignment, as reported in
     * https://github.com/boostorg/poly_collection/issues/16
     */

    if(!(Propagate&&!AlwaysEqual))
#endif
    BOOST_TEST(p3.get_allocator().comes_from(root2));
  }
  {
    rooted_poly_collection p2{root2};
    p2=cp;
    BOOST_TEST(p2==p);

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
    /* std::unordered_map copy assignment does not propagate allocators */
  
    if(!Propagate)
#endif
#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
    /* std::unordered_map copy assignment always and only propagates unequal
     * allocators.
     */

    if(!((Propagate&&AlwaysEqual)||(!Propagate&&!AlwaysEqual)))
#endif
#if !defined(BOOST_MSVC)&&\
    BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB,BOOST_TESTED_AT(804))
    /* std::unordered_map copy assignment does not propagate allocators, as
     * reported in https://github.com/boostorg/poly_collection/issues/16
     */

    if(!Propagate)
#endif
    BOOST_TEST(p2.get_allocator().comes_from(Propagate?root1:root2));
  }
#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
  /* std::unordered_map move asignment does not propagate allocators */

  if(!Propagate&&AlwaysEqual)
#endif
  {
    rooted_poly_collection p2{cp};
    auto                   d2=get_layout_data<Types...>(p2);
    rooted_poly_collection p3{root2};
    p3=std::move(p2);
    auto                   d3=get_layout_data<Types...>(p3);
    BOOST_TEST(p3==p);
    if(Propagate||AlwaysEqual){
      BOOST_TEST(d2==d3);
      BOOST_TEST(p2.empty());
      do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
    }

#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
    /* std::unordered_map move assignment always and only propagates unequal
     * allocators.
     */

    if(!((Propagate&&AlwaysEqual)||(!Propagate&&!AlwaysEqual)))
#endif
#if !defined(BOOST_MSVC)&&\
    BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB,BOOST_TESTED_AT(804))
    /* std::unordered_map move assignment does not propagate equal allocators,
     * as reported in https://github.com/boostorg/poly_collection/issues/16
     */

    if(!(Propagate&&AlwaysEqual))
#endif
    BOOST_TEST(p3.get_allocator().comes_from(Propagate?root1:root2));
  }
#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
  /* std::unordered_map::swap does not correctly swap control information when
   * swapping allocators, which causes crashes on "Checked Iterators" mode.
   */

  if(!(Propagate&&!AlwaysEqual))
#endif
  {
    constexpr bool use_same_allocator=!Propagate&&!AlwaysEqual;

    rooted_poly_collection p2{cp},
                           p3{use_same_allocator?root1:root2};

    auto d2=get_layout_data<Types...>(p2),
         d3=get_layout_data<Types...>(p3);

    p2.swap(p3);
    auto e2=get_layout_data<Types...>(p2),
         e3=get_layout_data<Types...>(p3);
    BOOST_TEST(d2==e3);
    BOOST_TEST(d3==e2);
    do_((BOOST_TEST(!p2.template is_registered<Types>()),0)...);
    if(!use_same_allocator
#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
       /* std::unordered_map::swap does not swap equal allocators */

       &&!(Propagate&&AlwaysEqual)
#endif
#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
       /* std::unordered_map::swap always and only swaps unequal allocators */

       &&!((Propagate&&AlwaysEqual)||(!Propagate&&!AlwaysEqual))
#endif
#if !defined(BOOST_MSVC)&&\
    BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB,BOOST_TESTED_AT(804))
      /* std::unordered_map::swap does not swap equal allocators, as reported
       * in https://github.com/boostorg/poly_collection/issues/16
       */

      &&!(Propagate&&AlwaysEqual)
#endif
    ){
      BOOST_TEST(p2.get_allocator().comes_from(Propagate?root2:root1));
      BOOST_TEST(p3.get_allocator().comes_from(Propagate?root1:root2));
    }

    using std::swap;
    swap(p2,p3);
    auto f2=get_layout_data<Types...>(p2),
         f3=get_layout_data<Types...>(p3);
    BOOST_TEST(e2==f3);
    BOOST_TEST(e3==f2);
    do_((BOOST_TEST(!p3.template is_registered<Types>()),0)...);
    if(!use_same_allocator){
      BOOST_TEST(p2.get_allocator().comes_from(root1));
      BOOST_TEST(p3.get_allocator().comes_from(root2));
    }
  }
}

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_construction()
{
  {
    constexpr bool propagate=true,always_equal=true;

    test_allocator_aware_construction<
      !propagate,!always_equal,PolyCollection,ValueFactory,Types...>();
    test_allocator_aware_construction<
      !propagate, always_equal,PolyCollection,ValueFactory,Types...>();
    test_allocator_aware_construction<
       propagate,!always_equal,PolyCollection,ValueFactory,Types...>();
    test_allocator_aware_construction<
       propagate, always_equal,PolyCollection,ValueFactory,Types...>();
  }

  {
    PolyCollection        p;
    const PolyCollection& cp=p;
    ValueFactory          v;

    fill<
      constraints<is_equality_comparable,is_copy_constructible>,
      Types...
    >(p,v,2);

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
}

void test_scoped_allocator()
{
#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<50000)&&\
    BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,>40704)
  /* std::scoped_allocator_adaptor not assignable, see
   * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65279 .
   * The bug prevents poly_collection below from creating any segment.
   */
#else
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
  auto& s=*p.begin<vector>();
  BOOST_TEST(p.get_allocator().comes_from(roote));

#if BOOST_WORKAROUND(BOOST_MSVC,>=1910)&&BOOST_WORKAROUND(BOOST_MSVC,<1916)
  /* https://developercommunity.visualstudio.com/content/problem/246251/
   *   3136309.html
   */
#else
  BOOST_TEST(s.get_allocator().comes_from(rootv));  
#endif
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
