/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_erasure.hpp"

#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename Type,typename PolyCollection>
void test_local_erase(const PolyCollection& p2)
{
  using size_type=typename PolyCollection::size_type;

  for(size_type i=0;i<p2.template size<Type>();++i){
    PolyCollection p=p2;
    auto it=p.erase(p.template cbegin<Type>()+i);
    BOOST_TEST(it-p.template begin<Type>()==(std::ptrdiff_t)i);
    BOOST_TEST(p.template size<Type>()==p2.template size<Type>()-1);
  }  
}

template<typename Type,typename PolyCollection>
void test_local_range_erase(const PolyCollection& p2)
{
  using size_type=typename PolyCollection::size_type;

  for(size_type i=0;i<=p2.template size<Type>();++i){
    for(size_type j=i;j<=p2.template size<Type>();++j){
      PolyCollection p=p2;
      auto first=p.template cbegin<Type>()+i,
           last=p.template cbegin<Type>()+j;
      auto it=p.erase(first,last);
      BOOST_TEST(it-p.template begin<Type>()==(std::ptrdiff_t)i);
      BOOST_TEST(p.template size<Type>()==p2.template size<Type>()-(j-i));
    }
  }
}

template<typename Type,typename PolyCollection>
void test_local_clear(const PolyCollection& p2)
{
  PolyCollection p=p2;
  p.template clear<Type>();
  BOOST_TEST(p.template empty<Type>());
  BOOST_TEST(p.size()==p2.size()-p2.template size<Type>());
}

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_erasure()
{
  using size_type=typename PolyCollection::size_type;

  PolyCollection p,p2;
  ValueFactory   v;

  fill<constraints<is_copy_constructible>,Types...>(p2,v,5);
  auto sit=p2.segment_traversal().begin();
  p2.clear(sit->type_info());
  ++sit;++sit;
  p2.clear(sit->type_info());

  for(size_type i=0;i<p2.size();++i){
    p=p2;
    auto it=p.erase(std::next(p.cbegin(),i));
    BOOST_TEST(std::distance(p.begin(),it)==(std::ptrdiff_t)i);
    BOOST_TEST(p.size()==p2.size()-1);
  }

  for(auto s:p2.segment_traversal()){
    auto& info=s.type_info();
    for(size_type i=0;i<p2.size(info);++i){
      p=p2;
      auto it=p.erase(p.cbegin(info)+i);
      BOOST_TEST(it-p.begin(info)==(std::ptrdiff_t)i);
      BOOST_TEST(p.size(info)==p2.size(info)-1);
    }
  }

  do_((
    p2.template is_registered<Types>()?test_local_erase<Types>(p2),0:0)...);

  for(size_type i=0;i<=p2.size();++i){
    for(size_type j=i;j<=p2.size();++j){
      p=p2;
      auto first=std::next(p.cbegin(),i),
            last=std::next(p.cbegin(),j);
      auto it=p.erase(first,last);
      BOOST_TEST(std::distance(p.begin(),it)==(std::ptrdiff_t)i);
      BOOST_TEST(p.size()==p2.size()-(j-i));
    }
  }

  for(auto s:p2.segment_traversal()){
    auto& info=s.type_info();
    for(size_type i=0;i<=p2.size(info);++i){
      for(size_type j=i;j<=p2.size(info);++j){
        p=p2;
        auto first=p.cbegin(info)+i,
              last=p.cbegin(info)+j;
        auto it=p.erase(first,last);
        BOOST_TEST(it-p.begin(info)==(std::ptrdiff_t)i);
        BOOST_TEST(p.size(info)==p2.size(info)-(j-i));
      }
    }
  }

  do_((p2.template is_registered<Types>()?
    test_local_range_erase<Types>(p2),0:0)...);

  p=p2;
  p.clear();
  BOOST_TEST(p.empty());

  for(auto s:p2.segment_traversal()){
    auto& info=s.type_info();
    p=p2;
    p.clear(info);
    BOOST_TEST(p.empty(info));
    BOOST_TEST(p.size()==p2.size()-p2.size(info));
  }

  do_((p2.template is_registered<Types>()?
    test_local_clear<Types>(p2),0:0)...);
}

void test_erasure()
{
  test_erasure<
    any_types::collection,auto_increment,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
  test_erasure<
    base_types::collection,auto_increment,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
  test_erasure<
    function_types::collection,auto_increment,
    function_types::t1,function_types::t2,function_types::t3,
    function_types::t4,function_types::t5>();
}
