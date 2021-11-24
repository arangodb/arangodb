/* Boost.MultiIndex test for node handling operations.
 *
 * Copyright 2003-2020 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_node_handling.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/core/enable_if.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/move/utility_core.hpp>
#include "pre_multi_index.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/next_prior.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <functional>
#include "count_allocator.hpp"

using namespace boost::multi_index;

void test_node_handle()
{
  typedef count_allocator<int>   allocator;
  typedef multi_index_container<
    int,
    indexed_by<
      ordered_unique<identity<int> >
    >,
    allocator
  >                              container;
  typedef container::node_type   node_type;

  std::size_t element_count=0,allocator_count=0;
  container   c((allocator(element_count,allocator_count)));

  element_count=0;   /* ignore non element-related allocations */
  allocator_count=0; /* ignore internal allocator(s) */

  c.insert(0);
  c.insert(1);
  c.insert(2);
  const int* addr0=&*c.find(0);
  const int* addr1=&*c.find(1);
  BOOST_TEST(element_count==3);

  node_type n1;
  BOOST_TEST(n1.empty());
  BOOST_TEST(!n1);
  BOOST_TEST(allocator_count==0);

  node_type n2=c.extract(0);
  BOOST_TEST(!n2.empty());
  BOOST_TEST((bool)n2);
  BOOST_TEST(n2.value()==0);
  BOOST_TEST(&n2.value()==addr0);
  BOOST_TEST(allocator_count==1);

  node_type n3(boost::move(n2));
  BOOST_TEST(n2.empty());
  BOOST_TEST(!n3.empty());
  BOOST_TEST(&n3.value()==addr0);
  BOOST_TEST(allocator_count==1);

  node_type n4(boost::move(n2));
  BOOST_TEST(n4.empty());
  BOOST_TEST(allocator_count==1);

  n1=boost::move(n3);
  BOOST_TEST(!n1.empty());
  BOOST_TEST(&n1.value()==addr0);
  BOOST_TEST(n3.empty());
  BOOST_TEST(allocator_count==1);

  BOOST_TEST(n1.get_allocator()==c.get_allocator());

  node_type n5=c.extract(1);
  BOOST_TEST(n5.value()==1);
  BOOST_TEST(&n5.value()==addr1);
  BOOST_TEST(allocator_count==2);

  n1.swap(n5);
  BOOST_TEST(&n1.value()==addr1);
  BOOST_TEST(&n5.value()==addr0);
  BOOST_TEST(allocator_count==2);

  using std::swap;

  swap(n2,n3);
  BOOST_TEST(n2.empty());
  BOOST_TEST(n3.empty());
  BOOST_TEST(allocator_count==2);

  swap(n1,n2);
  BOOST_TEST(!n2.empty());
  BOOST_TEST(&n2.value()==addr1);
  BOOST_TEST(allocator_count==2);

  swap(n1,n2);
  BOOST_TEST(&n1.value()==addr1);
  BOOST_TEST(n2.empty());
  BOOST_TEST(allocator_count==2);

  n2=boost::move(n3);
  BOOST_TEST(n2.empty());
  BOOST_TEST(n3.empty());
  BOOST_TEST(allocator_count==2);

  BOOST_TEST(element_count==3);
  n1=boost::move(n5);
  BOOST_TEST(&n1.value()==addr0);
  BOOST_TEST(n5.empty());
  BOOST_TEST(element_count==2);
  BOOST_TEST(allocator_count==1);

  n1=boost::move(n5);
  BOOST_TEST(n1.empty());
  BOOST_TEST(element_count==1);
  BOOST_TEST(allocator_count==0);

  c.extract(2);
  BOOST_TEST(element_count==0);
}

template<typename Index>
struct is_key_based:boost::integral_constant<
  bool,
  /* rather fragile if new index types are included in the library */
  (boost::tuples::length<typename Index::ctor_args>::value > 0)
>
{};

template<typename T>
struct is_iterator
{
  typedef char yes;
  struct no{char m[2];};

  template<typename Q> static no  test(...);
  template<typename Q> static yes test(typename Q::iterator_category*);

  BOOST_STATIC_CONSTANT(bool,value=(sizeof(test<T>(0))==sizeof(yes)));
};

template<typename T>
struct enable_if_not_iterator:boost::enable_if_c<
  !is_iterator<T>::value,
  void*
>{};

template<typename Dst,typename Ret,typename NodeHandle,typename Value>
void test_transfer_result(
  Dst&,Ret res,const NodeHandle& n,const Value& x,
  typename enable_if_not_iterator<Ret>::type=0)
{
  BOOST_TEST(*(res.position)==x);
  if(res.inserted){
    BOOST_TEST(res.node.empty());
  }
  else{
    BOOST_TEST(res.node.value()==x);
  }
  BOOST_TEST(n.empty());
}

template<typename Dst,typename NodeHandle,typename Value>
void test_transfer_result(
  Dst&,typename Dst::iterator res,const NodeHandle& n,const Value& x)
{
  BOOST_TEST(*res==x);
  if(n)BOOST_TEST(n.value()==x);
}

template<typename Dst,typename Ret>
void test_transfer_result_empty(
  Dst& dst,Ret res,
  typename enable_if_not_iterator<Ret>::type=0)
{
  BOOST_TEST(res.position==dst.end());
  BOOST_TEST(!res.inserted);
  BOOST_TEST(res.node.empty());
}

template<typename Dst>
void test_transfer_result_empty(Dst& dst,typename Dst::iterator res)
{
  BOOST_TEST(res==dst.end());
}

template<typename Dst,typename Ret,typename NodeHandle,typename Value>
void test_transfer_result(
  Dst& dst,typename Dst::iterator pos,Ret res,
  const NodeHandle& n,const Value& x,
  typename enable_if_not_iterator<Ret>::type=0)
{
  if(res.inserted&&pos!=dst.end()&&
    (!is_key_based<Dst>::value||*pos==x)){
    BOOST_TEST(boost::next(res.position)==pos);
  }
  test_transfer_result(dst,Ret(boost::move(res)),n,x);
}

template<typename Dst,typename NodeHandle,typename Value>
void test_transfer_result(
  Dst& dst,typename Dst::iterator pos,
  typename Dst::iterator res,const NodeHandle& n,const Value& x)
{
  if(n.empty()&&pos!=dst.end()&&
    (!is_key_based<Dst>::value||*pos==x)){
    BOOST_TEST(boost::next(res)==pos);
  }
  test_transfer_result(dst,res,n,x);
}

template<typename Dst,typename Ret>
void test_transfer_result_empty(
  Dst& dst,typename Dst::iterator,Ret res,
  typename enable_if_not_iterator<Ret>::type=0)
{
  test_transfer_result_empty(dst,Ret(boost::move(res)));
}

template<typename Dst>
void test_transfer_result_empty(
  Dst& dst,typename Dst::iterator,typename Dst::iterator res)
{
  test_transfer_result_empty(dst,res);
}

template<typename Src,typename Key>
typename Src::node_type checked_extract(
  Src& src,Key k,
  typename enable_if_not_iterator<Key>::type=0)
{
  typename Src::node_type n=src.extract(k);
  if(n)BOOST_TEST(src.key_extractor()(n.value())==k);
  return boost::move(n);
}

template<typename Src>
typename Src::node_type checked_extract(Src& src,typename Src::iterator pos)
{
  typename Src::value_type x=*pos;
  typename Src::node_type  n=src.extract(pos);
  if(n)BOOST_TEST(n.value()==x);
  return boost::move(n);
}

template<typename Src,typename Locator,typename Dst>
void test_transfer(Src& src,Locator loc,Dst& dst)
{
  typename Dst::node_type n=checked_extract(src,loc);
  if(n){
    typename Dst::value_type x=n.value();
    test_transfer_result(dst,dst.insert(boost::move(n)),n,x);
  }
  else{
    test_transfer_result_empty(dst,dst.insert(boost::move(n)));
  }
}

template<typename Src,typename Locator,typename Dst,typename Iterator>
void test_transfer(Src& src,Locator loc,Dst& dst,Iterator pos)
{
  typename Dst::node_type n=checked_extract(src,loc);
  if(n){
    typename Dst::value_type x=n.value();
    test_transfer_result(dst,pos,dst.insert(pos,boost::move(n)),n,x);
  }
  else{
    test_transfer_result_empty(dst,pos,dst.insert(pos,boost::move(n)));
  }
}

template<typename Src,typename Dst>
void test_transfer(
  Src& src,Dst& dst0,Dst& /* dst1 */,Dst& /* dst2 */,Dst& /* dst3 */,
  boost::false_type /* Src key-based */,boost::false_type /* Dst key-based */)
{
  test_transfer(src,src.begin(),dst0,dst0.begin());
  test_transfer(src,src.begin(),dst0,dst0.begin());
  for(int i=0;i<6;++i)src.extract(src.begin());
}

template<typename Src,typename Dst>
void test_transfer(
  Src& src,Dst& dst0,Dst& dst1,Dst& /* dst2 */,Dst& /* dst3 */,
  boost::false_type /* Src key-based */,boost::true_type /* Dst key-based */)
{
  test_transfer(src,src.begin(),dst0);
  test_transfer(src,src.begin(),dst0);
  test_transfer(src,src.begin(),dst1,dst1.find(*src.begin()));
  test_transfer(src,src.begin(),dst1,dst1.find(*src.begin()));
  for(int i=0;i<4;++i)src.extract(src.begin());
}

template<typename Src,typename Dst>
void test_transfer(
  Src& src,Dst& dst0,Dst& dst1,Dst& /* dst2 */,Dst& /* dst3 */,
  boost::true_type /* Src key-based */,boost::false_type /* Dst key-based */)
{
  test_transfer(src, src.begin(),dst0,dst0.begin());
  test_transfer(src, src.begin(),dst0,dst0.begin());
  test_transfer(src,*src.begin(),dst1,dst1.begin());
  test_transfer(src,*src.begin(),dst1,dst1.begin());
  test_transfer(src,          -1,dst1,dst1.begin());
  for(int i=0;i<4;++i)src.extract(src.begin());
}

template<typename Src,typename Dst>
void test_transfer(
  Src& src,Dst& dst0,Dst& dst1,Dst& dst2,Dst& dst3,
  boost::true_type /* Src key-based */,boost::true_type /* Dst key-based */)
{
  test_transfer(src, src.begin(),dst0);
  test_transfer(src, src.begin(),dst0);
  test_transfer(src,*src.begin(),dst1);
  test_transfer(src,*src.begin(),dst1);
  test_transfer(src,          -1,dst1);
  test_transfer(src, src.begin(),dst2,dst2.find(*src.begin()));
  test_transfer(src, src.begin(),dst2,dst2.find(*src.begin()));
  test_transfer(src,*src.begin(),dst3,dst3.find(*src.begin()));
  test_transfer(src,*src.begin(),dst3,dst3.find(*src.begin()));
  test_transfer(src,          -1,dst3,dst3.begin());
}

template<typename Src,typename Dst>
void test_transfer(Src& src,Dst& dst0,Dst& dst1,Dst& dst2,Dst& dst3)
{
  test_transfer(
    src,dst0,dst1,dst2,dst3,is_key_based<Src>(),is_key_based<Dst>());
}

void test_transfer()
{
  typedef multi_index_container<
    int,
    indexed_by<
      hashed_non_unique<identity<int> >,
      ordered_non_unique<identity<int> >,
      random_access<>,
      sequenced<>,
      ranked_non_unique<identity<int> >
    >
  >                                                     container1;
  typedef multi_index_container<
    int,
    indexed_by<
      hashed_non_unique<identity<int> >,
      ordered_unique<identity<int>,std::greater<int> >,
      random_access<>,
      sequenced<>,
      ranked_unique<identity<int>,std::greater<int> >
    >
  >                                                     container2;

  container1                      src;
  container1::nth_index<0>::type& src0=src.get<0>();
  container1::nth_index<1>::type& src1=src.get<1>();
  container1::nth_index<2>::type& src2=src.get<2>();
  container1::nth_index<3>::type& src3=src.get<3>();
  container1::nth_index<4>::type& src4=src.get<4>();
  container2                      dst0,dst1,dst2,dst3;
  container2::nth_index<0>::type& dst00=dst0.get<0>(),
                                & dst10=dst1.get<0>(),
                                & dst20=dst2.get<0>(),
                                & dst30=dst3.get<0>();
  container2::nth_index<1>::type& dst01=dst0.get<1>(),
                                & dst11=dst1.get<1>(),
                                & dst21=dst2.get<1>(),
                                & dst31=dst3.get<1>();
  container2::nth_index<2>::type& dst02=dst0.get<2>(),
                                & dst12=dst1.get<2>(),
                                & dst22=dst2.get<2>(),
                                & dst32=dst3.get<2>();
  container2::nth_index<3>::type& dst03=dst0.get<3>(),
                                & dst13=dst1.get<3>(),
                                & dst23=dst2.get<3>(),
                                & dst33=dst3.get<3>();
  container2::nth_index<4>::type& dst04=dst0.get<4>(),
                                & dst14=dst1.get<4>(),
                                & dst24=dst2.get<4>(),
                                & dst34=dst3.get<4>();
  for(int i=0;i<6;++i){
    for(int j=0;j<8;++j)src.insert(i);
  }

  test_transfer(src0,dst01,dst11,dst21,dst31);
  test_transfer(src1,dst02,dst12,dst22,dst32);
  test_transfer(src2,dst03,dst13,dst23,dst33);
  test_transfer(src3,dst04,dst14,dst24,dst34);
  test_transfer(src4,dst00,dst10,dst20,dst30);
  BOOST_TEST(src.size()==8);
}

void test_node_handling()
{
  test_node_handle();
  test_transfer();
}
