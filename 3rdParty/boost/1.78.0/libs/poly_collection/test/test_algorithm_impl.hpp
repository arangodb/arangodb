/* Copyright 2016-2020 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_TEST_TEST_ALGORITHM_IMPL_HPP
#define BOOST_POLY_COLLECTION_TEST_TEST_ALGORITHM_IMPL_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <algorithm>
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/iterator/function_output_iterator.hpp>
#include <boost/poly_collection/algorithm.hpp>
#include <functional>
#include <iterator>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>
#include "test_utilities.hpp"

using namespace test_utilities;

#if BOOST_WORKAROUND(BOOST_MSVC,>=1910)
/* https://lists.boost.org/Archives/boost/2017/06/235687.php */

#define DEFINE_ALGORITHM(name,f)                    \
template<typename... Ts>                            \
struct name                                         \
{                                                   \
  template<typename... Args>                        \
  auto operator()(Args&&... args)const              \
  {                                                 \
    return f<Ts...>(std::forward<Args>(args)...);   \
  }                                                 \
};
#elif BOOST_WORKAROUND(BOOST_GCC_VERSION,<50200)
/* problem here is with return type containing <Ts...> */

#define DEFINE_ALGORITHM(name,f)                    \
template<typename... Ts>                            \
struct name                                         \
{                                                   \
  template<typename... Args>                        \
  auto operator()(Args&&... args)const->            \
    decltype(f(std::forward<Args>(args)...))        \
  {                                                 \
    return f<Ts...>(std::forward<Args>(args)...);   \
  }                                                 \
};
#else
#define DEFINE_ALGORITHM(name,f)                    \
template<typename... Ts>                            \
struct name                                         \
{                                                   \
  template<typename... Args>                        \
  auto operator()(Args&&... args)const->            \
    decltype(f<Ts...>(std::forward<Args>(args)...)) \
  {                                                 \
    return f<Ts...>(std::forward<Args>(args)...);   \
  }                                                 \
};
#endif

DEFINE_ALGORITHM(std_all_of,std::all_of)
DEFINE_ALGORITHM(poly_all_of,boost::poly_collection::all_of)
DEFINE_ALGORITHM(std_any_of,std::any_of)
DEFINE_ALGORITHM(poly_any_of,boost::poly_collection::any_of)
DEFINE_ALGORITHM(std_none_of,std::none_of)
DEFINE_ALGORITHM(poly_none_of,boost::poly_collection::none_of)
DEFINE_ALGORITHM(std_for_each,std::for_each)
DEFINE_ALGORITHM(poly_for_each,boost::poly_collection::for_each)
DEFINE_ALGORITHM(poly_for_each_n,boost::poly_collection::for_each_n)
DEFINE_ALGORITHM(std_find,std::find)
DEFINE_ALGORITHM(poly_find,boost::poly_collection::find)
DEFINE_ALGORITHM(std_find_if,std::find_if)
DEFINE_ALGORITHM(poly_find_if,boost::poly_collection::find_if)
DEFINE_ALGORITHM(std_find_if_not,std::find_if_not)
DEFINE_ALGORITHM(poly_find_if_not,boost::poly_collection::find_if_not)
DEFINE_ALGORITHM(std_find_end,std::find_end)
DEFINE_ALGORITHM(poly_find_end,boost::poly_collection::find_end)
DEFINE_ALGORITHM(std_find_first_of,std::find_first_of)
DEFINE_ALGORITHM(poly_find_first_of,boost::poly_collection::find_first_of)
DEFINE_ALGORITHM(std_adjacent_find,std::adjacent_find)
DEFINE_ALGORITHM(poly_adjacent_find,boost::poly_collection::adjacent_find)
DEFINE_ALGORITHM(std_count,std::count)
DEFINE_ALGORITHM(poly_count,boost::poly_collection::count)
DEFINE_ALGORITHM(std_count_if,std::count_if)
DEFINE_ALGORITHM(poly_count_if,boost::poly_collection::count_if)
DEFINE_ALGORITHM(std_cpp11_mismatch,std::mismatch) /* note std_cpp11 prefix */
DEFINE_ALGORITHM(poly_mismatch,boost::poly_collection::mismatch)
DEFINE_ALGORITHM(std_cpp11_equal,std::equal)       /* note std_cpp11 prefix */
DEFINE_ALGORITHM(poly_equal,boost::poly_collection::equal)
DEFINE_ALGORITHM(std_cpp11_is_permutation,std::is_permutation) /* std_cpp11 */
DEFINE_ALGORITHM(poly_is_permutation,boost::poly_collection::is_permutation)
DEFINE_ALGORITHM(std_search,std::search)
DEFINE_ALGORITHM(poly_search,boost::poly_collection::search)
DEFINE_ALGORITHM(std_search_n,std::search_n)
DEFINE_ALGORITHM(poly_search_n,boost::poly_collection::search_n)
DEFINE_ALGORITHM(std_copy,std::copy)
DEFINE_ALGORITHM(poly_copy,boost::poly_collection::copy)
DEFINE_ALGORITHM(std_copy_n,std::copy_n)
DEFINE_ALGORITHM(poly_copy_n,boost::poly_collection::copy_n)
DEFINE_ALGORITHM(std_copy_if,std::copy_if)
DEFINE_ALGORITHM(poly_copy_if,boost::poly_collection::copy_if)
DEFINE_ALGORITHM(std_move,std::move)
DEFINE_ALGORITHM(poly_move,boost::poly_collection::move)
DEFINE_ALGORITHM(std_transform,std::transform)
DEFINE_ALGORITHM(poly_transform,boost::poly_collection::transform)
DEFINE_ALGORITHM(std_replace_copy,std::replace_copy)
DEFINE_ALGORITHM(poly_replace_copy,boost::poly_collection::replace_copy)
DEFINE_ALGORITHM(std_replace_copy_if,std::replace_copy_if)
DEFINE_ALGORITHM(poly_replace_copy_if,boost::poly_collection::replace_copy_if)
DEFINE_ALGORITHM(std_remove_copy,std::remove_copy)
DEFINE_ALGORITHM(poly_remove_copy,boost::poly_collection::remove_copy)
DEFINE_ALGORITHM(std_remove_copy_if,std::remove_copy_if)
DEFINE_ALGORITHM(poly_remove_copy_if,boost::poly_collection::remove_copy_if)
DEFINE_ALGORITHM(std_unique_copy,std::unique_copy)
DEFINE_ALGORITHM(poly_unique_copy,boost::poly_collection::unique_copy)
DEFINE_ALGORITHM(poly_sample,boost::poly_collection::sample)
DEFINE_ALGORITHM(std_rotate_copy,std::rotate_copy)
DEFINE_ALGORITHM(poly_rotate_copy,boost::poly_collection::rotate_copy)
DEFINE_ALGORITHM(std_is_partitioned,std::is_partitioned)
DEFINE_ALGORITHM(poly_is_partitioned,boost::poly_collection::is_partitioned)
DEFINE_ALGORITHM(std_partition_copy,std::partition_copy)
DEFINE_ALGORITHM(poly_partition_copy,boost::poly_collection::partition_copy)
DEFINE_ALGORITHM(std_partition_point,std::partition_point)
DEFINE_ALGORITHM(poly_partition_point,boost::poly_collection::partition_point)

template<typename...>
struct std_for_each_n
{
  /* implemented here as the algorithm is C++17 */

  template<typename InputIterator,typename Size,typename Function>
  InputIterator operator()(InputIterator first,Size n,Function f)const
  {
    for(;n>0;++first,(void)--n)f(*first);
    return first;
  }
};

template<typename... Ts>
struct std_mismatch:std_cpp11_mismatch<Ts...>
{
  using std_cpp11_mismatch<Ts...>::operator();

  /* C++14 variants */

  template<typename InputIterator1,typename InputIterator2>
  std::pair<InputIterator1,InputIterator2> operator()(
    InputIterator1 first1,InputIterator1 last1,
    InputIterator2 first2,InputIterator2 last2)const
  {
    while(first1!=last1&&first2!=last2&&*first1==*first2){
      ++first1;
      ++first2;
    }
    return {first1,first2};
  }

  template<typename InputIterator1,typename InputIterator2,typename Predicate>
  std::pair<InputIterator1,InputIterator2> operator()(
    InputIterator1 first1,InputIterator1 last1,
    InputIterator2 first2,InputIterator2 last2,Predicate pred)const
  {
    while(first1!=last1&&first2!=last2&&pred(*first1,*first2)){
      ++first1;
      ++first2;
    }
    return {first1,first2};
  }
};

template<typename... Ts>
struct std_equal:std_cpp11_equal<Ts...>
{
  using std_cpp11_equal<Ts...>::operator();

  /* C++14 variants */

  template<typename InputIterator1,typename InputIterator2>
  bool operator()(
    InputIterator1 first1,InputIterator1 last1,
    InputIterator2 first2,InputIterator2 last2)const
  {
    for(;first1!=last1&&first2!=last2;++first1,++first2){
      if(!(*first1==*first2))return false;
    }
    return first1==last1&&first2==last2;
  }

  template<typename InputIterator1,typename InputIterator2,typename Predicate>
  bool operator()(
    InputIterator1 first1,InputIterator1 last1,
    InputIterator2 first2,InputIterator2 last2,Predicate pred)const
  {
    for(;first1!=last1&&first2!=last2;++first1,++first2){
      if(!pred(*first1,*first2))return false;
    }
    return first1==last1&&first2==last2;
  }
};

template<typename... Ts>
struct std_is_permutation:std_cpp11_is_permutation<Ts...>
{
  using std_cpp11_is_permutation<Ts...>::operator();

  /* The implementation of predicate-based std::is_permutation in GCC<=4.8
   * version of libstdc++-v3 incorrectly triggers the instantiation of
   * ForwardIterator2::value_type, which fails when this is an abstract class.
   * The implementation below ripped from libc++ source code.
   */

  template<
    typename ForwardIterator1,typename ForwardIterator2,typename Predicate
  >
  bool operator()(
    ForwardIterator1 first1,ForwardIterator1 last1,
    ForwardIterator2 first2,Predicate pred)const
  {
    using difference_type=
      typename std::iterator_traits<ForwardIterator1>::difference_type;

    for(;first1!=last1;++first1,(void)++first2){
      if(!pred(*first1,*first2))goto not_done;
    }
    return true;

  not_done:
    difference_type l1=std::distance(first1,last1);
    if(l1==difference_type(1))return false;

    ForwardIterator2 last2=std::next(first2,l1);
    for(ForwardIterator1 i=first1;i!= last1;++i){
      for(ForwardIterator1 j=first1;j!=i;++j)if(pred(*j,*i))goto next_iter;
      {
        difference_type c2=0;
        for(ForwardIterator2 j=first2;j!=last2;++j)if(pred(*i,*j))++c2;
        if(c2==0)return false;
        difference_type c1=1;
        for(ForwardIterator1 j=std::next(i);j!=last1;++j)if(pred(*i,*j))++c1;
        if(c1!=c2)return false;
      }
  next_iter:;
    }
    return true;
  }

  /* C++14 variants */

  template<typename ForwardIterator1,typename ForwardIterator2>
  bool operator()(
    ForwardIterator1 first1,ForwardIterator1 last1,
    ForwardIterator2 first2,ForwardIterator2 last2)const
  {
    if(std::distance(first1,last1)!=std::distance(first2,last2))return false;
    return (*this)(first1,last1,first2);
  }

  template<
    typename ForwardIterator1,typename ForwardIterator2,typename Predicate
  >
  bool operator()(
    ForwardIterator1 first1,ForwardIterator1 last1,
    ForwardIterator2 first2,ForwardIterator2 last2,Predicate pred)const
  {
    if(std::distance(first1,last1)!=std::distance(first2,last2))return false;
    return (*this)(first1,last1,first2,pred);
  }
};

template<typename...>
struct std_sample
{
  /* Implemented here as the algorithm is C++17 and because we need to control
   * the implementation to do white-box testing. Only the ForwardIterator
   * version required.
   */

  template<
    typename ForwardIterator,typename OutputIterator,
    typename Distance, typename UniformRandomBitGenerator
  >
  OutputIterator operator()(
    ForwardIterator first,ForwardIterator last,
    OutputIterator res, Distance n,UniformRandomBitGenerator&& g)const
  {
    Distance m=std::distance(first,last);
    for(n=(std::min)(n,m);n!=0;++first){
      auto r=std::uniform_int_distribution<Distance>(0,--m)(g);
      if (r<n){
        *res++=*first;
        --n;
      }
    }
    return res;
  }
};

template<
  typename ControlAlgorithm,typename Algorithm,
  typename PolyCollection,typename... Args
>
void test_algorithm(PolyCollection& p,Args&&... args)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(auto last=first;;++last){
      BOOST_TEST(
        alg(first,last,std::forward<Args>(args)...)==
        control(first,last,std::forward<Args>(args)...));
      if(last==end)break;
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename PolyCollection,typename... Args
>
void test_algorithms(PolyCollection& p,Args&&... args)
{
  do_((test_algorithm<ControlAlgorithm,Algorithms>(
    p,std::forward<Args>(args)...),0)...);
  do_((test_algorithm<ControlAlgorithm,Algorithms>(
    const_cast<const PolyCollection&>(p),std::forward<Args>(args)...),0)...);
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename PolyCollection,typename... Args
>
void test_algorithms_with_equality_impl(
  std::true_type,PolyCollection& p,Args&&... args)
{
  test_algorithms<ControlAlgorithm,Algorithms...>(
    p,std::forward<Args>(args)...);
}

template<
  typename ControlAlgorithm,typename... Algorithm,
  typename PolyCollection,typename... Args
>
void test_algorithms_with_equality_impl(
  std::false_type,PolyCollection&,Args&&...)
{
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename PolyCollection,typename... Args
>
void test_algorithms_with_equality(PolyCollection& p,Args&&... args)
{
  test_algorithms_with_equality_impl<ControlAlgorithm,Algorithms...>(
    is_equality_comparable<typename PolyCollection::value_type>{},
    p,std::forward<Args>(args)...);
}

template<
  typename ControlAlgorithm,typename Algorithm,
  typename ToInt,typename PolyCollection
>
void test_for_each_n_algorithm(ToInt to_int,PolyCollection& p)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(auto n=std::distance(first,end);n>=0;--n){
      int  res1=0,res2=0;
      auto acc1=compose(to_int,[&](int x){res1+=x;});
      auto acc2=compose(to_int,[&](int x){res2+=x;});
      auto it1=alg(first,n,acc1),
           it2=control(first,n,acc2);
      BOOST_TEST(it1==it2);
      BOOST_TEST(res1==res2);
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection
>
void test_for_each_n_algorithms(ToInt to_int,PolyCollection& p)
{
  do_((test_for_each_n_algorithm<ControlAlgorithm,Algorithms>(
    to_int,p),0)...);
  do_((test_for_each_n_algorithm<ControlAlgorithm,Algorithms>(
    to_int,const_cast<const PolyCollection&>(p)),0)...);
}

template<
  typename ControlAlgorithm,typename Algorithm,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_algorithm(ToInt to_int,PolyCollection& p,Args&&... args)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(auto last=first;;++last){
      using vector=std::vector<int>;

      vector v1,v2;
      auto   insert1=compose(to_int,[&](int x){v1.push_back(x);});
      auto   insert2=compose(to_int,[&](int x){v2.push_back(x);});
      auto   out1=boost::make_function_output_iterator(std::ref(insert1));
      auto   out2=boost::make_function_output_iterator(std::ref(insert2));

      out1=alg(first,last,out1,std::forward<Args>(args)...);
      out2=control(first,last,out2,std::forward<Args>(args)...);
      BOOST_TEST(v1==v2);
      if(last==end)break;
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_algorithms(ToInt to_int,PolyCollection& p,Args&&... args)
{
  do_((test_copy_algorithm<ControlAlgorithm,Algorithms>(
    to_int,p,std::forward<Args>(args)...),0)...);
  do_((test_copy_algorithm<ControlAlgorithm,Algorithms>(
    to_int,const_cast<const PolyCollection&>(p),
    std::forward<Args>(args)...),0)...);
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_algorithms_with_equality_impl(
  std::true_type,ToInt to_int,PolyCollection& p,Args&&... args)
{
  test_copy_algorithms<ControlAlgorithm,Algorithms...>(
    to_int,p,std::forward<Args>(args)...);
}

template<
  typename ControlAlgorithm,typename... Algorithm,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_algorithms_with_equality_impl(
  std::false_type,ToInt,PolyCollection&,Args&&...)
{
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_algorithms_with_equality(
  ToInt to_int,PolyCollection& p,Args&&... args)
{
  test_copy_algorithms_with_equality_impl<ControlAlgorithm,Algorithms...>(
    is_equality_comparable<typename PolyCollection::value_type>{},
    to_int,p,std::forward<Args>(args)...);
}

template<
  typename ControlAlgorithm,typename Algorithm,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_n_algorithm(ToInt to_int,PolyCollection& p,Args&&... args)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(std::ptrdiff_t n=0,m=std::distance(first,end);n<=m;++n){
      using vector=std::vector<int>;

      vector v1,v2;
      auto   insert1=compose(to_int,[&](int x){v1.push_back(x);});
      auto   insert2=compose(to_int,[&](int x){v2.push_back(x);});
      auto   out1=boost::make_function_output_iterator(std::ref(insert1));
      auto   out2=boost::make_function_output_iterator(std::ref(insert2));

      alg(first,n,out1,std::forward<Args>(args)...);
      control(first,n,out2,std::forward<Args>(args)...);
      BOOST_TEST(v1==v2);
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection,typename... Args
>
void test_copy_n_algorithms(ToInt to_int,PolyCollection& p,Args&&... args)
{
  do_((test_copy_n_algorithm<ControlAlgorithm,Algorithms>(
    to_int,p,std::forward<Args>(args)...),0)...);
  do_((test_copy_n_algorithm<ControlAlgorithm,Algorithms>(
    to_int,const_cast<const PolyCollection&>(p),
    std::forward<Args>(args)...),0)...);
}

template<
  typename ControlAlgorithm,typename Algorithm,
  typename ToInt,typename PolyCollection
>
void test_transform2_algorithm(ToInt to_int,PolyCollection& p)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(auto last=first;;++last){
      using vector=std::vector<int>;

      auto   op=compose_all(to_int,[](int x,int y){return x+y;});
      vector v1,v2;
      auto   insert1=[&](int x){v1.push_back(x);};
      auto   insert2=[&](int x){v2.push_back(x);};
      auto   out1=boost::make_function_output_iterator(std::ref(insert1));
      auto   out2=boost::make_function_output_iterator(std::ref(insert2));

      out1=alg(first,last,p.begin(),out1,op);
      out2=control(first,last,p.begin(),out2,op);
      BOOST_TEST(v1==v2);
      if(last==end)break;
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection
>
void test_transform2_algorithms(ToInt to_int,PolyCollection& p)
{
  do_((test_transform2_algorithm<ControlAlgorithm,Algorithms>(
    to_int,p),0)...);
  do_((test_transform2_algorithm<ControlAlgorithm,Algorithms>(
    to_int,const_cast<const PolyCollection&>(p)),0)...);
}

template<
  typename ControlAlgorithm,typename Algorithm,
  typename ToInt,typename PolyCollection
>
void test_rotate_copy_algorithm(ToInt to_int,PolyCollection& p)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(auto last=first;;++last){
      for(auto middle=first;;++middle){
        using vector=std::vector<int>;

        vector v1,v2;
        auto   insert1=compose(to_int,[&](int x){v1.push_back(x);});
        auto   insert2=compose(to_int,[&](int x){v2.push_back(x);});
        auto   out1=boost::make_function_output_iterator(std::ref(insert1));
        auto   out2=boost::make_function_output_iterator(std::ref(insert2));

        out1=alg(first,middle,last,out1);
        out2=control(first,middle,last,out2);
        BOOST_TEST(v1==v2);

        if(middle==last)break;
      }
      if(last==end)break;
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection
>
void test_rotate_copy_algorithms(ToInt to_int,PolyCollection& p)
{
  do_((test_rotate_copy_algorithm<ControlAlgorithm,Algorithms>(
    to_int,p),0)...);
  do_((test_rotate_copy_algorithm<ControlAlgorithm,Algorithms>(
    to_int,const_cast<const PolyCollection&>(p)),0)...);
}

template<
  typename ControlAlgorithm,typename Algorithm,
  typename ToInt,typename PolyCollection,typename Predicate
>
void test_partition_copy_algorithm(
  ToInt to_int,PolyCollection& p,Predicate pred)
{
  Algorithm        alg;
  ControlAlgorithm control;
  for(auto first=p.begin(),end=p.end();;++first){
    for(auto last=first;;++last){
      using vector=std::vector<int>;

      vector v11,v12,v21,v22;
      auto   insert11=compose(to_int,[&](int x){v11.push_back(x);});
      auto   insert12=compose(to_int,[&](int x){v12.push_back(x);});
      auto   insert21=compose(to_int,[&](int x){v21.push_back(x);});
      auto   insert22=compose(to_int,[&](int x){v22.push_back(x);});
      auto   out11=boost::make_function_output_iterator(std::ref(insert11));
      auto   out12=boost::make_function_output_iterator(std::ref(insert12));
      auto   out21=boost::make_function_output_iterator(std::ref(insert21));
      auto   out22=boost::make_function_output_iterator(std::ref(insert22));

      std::tie(out11,out12)=alg(first,last,out11,out12,pred);
      std::tie(out21,out22)=control(first,last,out21,out22,pred);
      BOOST_TEST(v11==v21);
      BOOST_TEST(v12==v22);
      if(last==end)break;
    }
    if(first==end)break;
  }
}

template<
  typename ControlAlgorithm,typename... Algorithms,
  typename ToInt,typename PolyCollection,typename Predicate
>
void test_partition_copy_algorithms(
  ToInt to_int,PolyCollection& p,Predicate pred)
{
  do_((test_partition_copy_algorithm<ControlAlgorithm,Algorithms>(
    to_int,p,pred),0)...);
  do_((test_partition_copy_algorithm<ControlAlgorithm,Algorithms>(
    to_int,const_cast<const PolyCollection&>(p),pred),0)...);
}

template<typename ToInt>
struct poly_accumulator_class
{
  poly_accumulator_class(const ToInt& to_int):res{0},to_int(to_int){}
  bool operator==(const poly_accumulator_class& x)const{return res==x.res;}

  template<typename T> void operator()(const T& x){res+=to_int(x);}

  int   res;
  ToInt to_int;
};

template<typename ToInt>
poly_accumulator_class<ToInt> poly_accumulator(const ToInt& to_int)
{
  return to_int;
}

template<typename Algorithm>
struct force_arg_copying
{
  Algorithm alg;

  template<typename T>
  static T copy(const T& x){return x;}

  template<typename... Args>
  auto operator()(Args&&... args)const
    ->decltype(std::declval<Algorithm>()(copy(args)...))
  {
    return alg(copy(args)...);
  }
};

template<
  typename PolyCollection,typename ValueFactory,typename ToInt,
  typename... Types
>
void test_algorithm()
{
  PolyCollection p;
  ValueFactory   v;
  ToInt          to_int;

  fill<constraints<>,Types...>(p,v,2);

  {
    auto always_true=compose(to_int,[](int){return true;});
    auto always_false=compose(to_int,[](int){return false;});
    auto pred=compose(to_int,[](int x){return x%2==0;});

    test_algorithms<std_all_of<>,poly_all_of<>,poly_all_of<Types...>>(
      p,always_true);
    test_algorithms<std_all_of<>,poly_all_of<>,poly_all_of<Types...>>(
      p,always_false);
    test_algorithms<std_all_of<>,poly_all_of<>,poly_all_of<Types...>>(
      p,pred);

    test_algorithms<std_any_of<>,poly_any_of<>,poly_any_of<Types...>>(
      p,always_true);
    test_algorithms<std_any_of<>,poly_any_of<>,poly_any_of<Types...>>(
      p,always_false);
    test_algorithms<std_any_of<>,poly_any_of<>,poly_any_of<Types...>>(
      p,pred);

    test_algorithms<std_none_of<>,poly_none_of<>,poly_none_of<Types...>>(
      p,always_true);
    test_algorithms<std_none_of<>,poly_none_of<>,poly_none_of<Types...>>(
      p,always_false);
    test_algorithms<std_none_of<>,poly_none_of<>,poly_none_of<Types...>>(
      p,pred);
  }
  {
    test_algorithms<std_for_each<>,poly_for_each<>,poly_for_each<Types...>>(
      p,poly_accumulator(to_int));

    test_for_each_n_algorithms<
      std_for_each_n<>,poly_for_each_n<>,poly_for_each_n<Types...>
    >(to_int,p);
  }
  {
    for(const auto& x:p){
      test_algorithms_with_equality<
        std_find<>,poly_find<>,only_eq_comparable<poly_find,Types...>
      >(p,x);
    }
  }
  {
    auto it=p.begin();
    std::advance(it,p.size()/2);
    int n=to_int(*it);
    auto pred=compose(to_int,[=](int x){return x==n;});

    test_algorithms<std_find_if<>,poly_find_if<>,poly_find_if<Types...>>(
      p,pred);

    test_algorithms<
      std_find_if_not<>,poly_find_if_not<>,poly_find_if_not<Types...>
    >(p,pred);
  }
  {
    auto first=p.begin(),end=first;
    std::advance(end,+p.size()/2);
    for(;first!=end;++first){
      test_algorithms_with_equality<
        std_find_end<>,poly_find_end<>,
        only_eq_comparable<poly_find_end,Types...>
      >(p,first,end);

      test_algorithms_with_equality<
        std_search<>,poly_search<>,
        only_eq_comparable<poly_search,Types...>
      >(p,first,end);
    }
  }
  {
    std::vector<int> v;
    for(const auto& x:p)v.push_back(to_int(x));
    v.erase(v.begin()+v.size()/2,v.end());

    for(auto first=v.begin(),end=v.begin()+v.size()/2;first!=end;++first){
      for(int i=1;i<4;++i){
        auto pred=compose(to_int,[&](int x,int y){return x%i==y%i;});

        test_algorithms<
          std_find_end<>,poly_find_end<>,poly_find_end<Types...>
        >(p,first,end,pred);

        test_algorithms<std_search<>,poly_search<>,poly_search<Types...>>(
          p,first,end,pred);
      }
    }
  }
  {
    using value_type=typename PolyCollection::value_type;
    using vector=std::vector<std::reference_wrapper<const value_type>>;

    vector v;
    for(const auto& x:p){
      switch(v.size()%3){
        case 0:v.push_back(x);break;
        case 1:v.push_back(*p.begin());break;
        default:{}
      }
    }

    for(auto first=v.begin(),end=v.end();;++first){
      for(auto last=first;;++last){
        test_algorithms_with_equality<
          std_find_first_of<>,poly_find_first_of<>,
          only_eq_comparable<poly_find_first_of,Types...>
        >(p,first,last);        
        if(last==end)break;
      }
      if(first==end)break;
    }
  }
  {
    std::vector<int> v;
    for(const auto& x:p){
      switch(v.size()%3){
        case 0:v.push_back(to_int(x));break;
        case 1:v.push_back(-1);break;
        default:{}
      }
    }

    for(auto first=v.begin(),end=v.end();;++first){
      for(auto last=first;;++last){
        test_algorithms<
          std_find_first_of<>,poly_find_first_of<>,poly_find_first_of<Types...>
        >(p,first,last,compose(to_int,std::equal_to<int>{}));        
        if(last==end)break;
      }
      if(first==end)break;
    }
  }
  {
    test_algorithms_with_equality<
      std_adjacent_find<>,poly_adjacent_find<>,
      only_eq_comparable<poly_adjacent_find,Types...>
    >(p);        
  }
  {
    std::vector<int> v;
    for(const auto& x:p)v.push_back(to_int(x));
    v.push_back(-1);

    for(auto first=v.begin(),end=v.end()-1;first!=end;++first){
      int n1=*first,n2=*(first+1);
      test_algorithms<
        std_adjacent_find<>,poly_adjacent_find<>,poly_adjacent_find<Types...>
      >(p,compose_all(to_int,[=](int x,int y){return x==n1&&y==n2;}));        
    }
  }
  {
    for(const auto& x:p){
      test_algorithms_with_equality<
        std_count<>,poly_count<>,only_eq_comparable<poly_count,Types...>
      >(p,x);
    }
  }
  {
    for(int i=1;i<4;++i){
      test_algorithms<std_count_if<>,poly_count_if<>,poly_count_if<Types...>>(
        p,compose(to_int,[&](int x){return x%i==0;}));
    }
  }
  {
    using value_type=typename PolyCollection::value_type;
    using vector=std::vector<std::reference_wrapper<const value_type>>;

    vector v;
    for(const auto& x:p)v.push_back(x);
    v.back()=v.front();
    auto w=v;
    v.insert(v.end(),w.begin(),w.end());

    for(auto first=v.begin(),end=v.begin()+v.size()/2;first!=end;++first){
      test_algorithms_with_equality<
        std_mismatch<>,poly_mismatch<>,
        only_eq_comparable<poly_mismatch,Types...>
      >(p,first);

      test_algorithms_with_equality<
        std_mismatch<>,poly_mismatch<>,
        only_eq_comparable<poly_mismatch,Types...>
      >(p,first,end);

      test_algorithms_with_equality<
        std_equal<>,poly_equal<>,only_eq_comparable<poly_equal,Types...>
      >(p,first);

      test_algorithms_with_equality<
        std_equal<>,poly_equal<>,only_eq_comparable<poly_equal,Types...>
      >(p,first,end);
    }

    test_algorithms_with_equality<
      std_mismatch<>,poly_mismatch<>,only_eq_comparable<poly_mismatch,Types...>
    >(p,v.end(),v.end());

    test_algorithms_with_equality<
      std_equal<>,poly_equal<>,only_eq_comparable<poly_equal,Types...>
    >(p,v.end(),v.end());
  }
  {
    std::vector<int> v;
    for(const auto& x:p)v.push_back(to_int(x));
    v.back()=-1;
    auto w=v;
    v.insert(v.end(),w.begin(),w.end());
    auto pred=compose(to_int,std::equal_to<int>{});

    for(auto first=v.begin(),end=v.begin()+v.size()/2;first!=end;++first){
      test_algorithms<std_mismatch<>,poly_mismatch<>,poly_mismatch<Types...>>(
        p,first,pred);

      test_algorithms<std_mismatch<>,poly_mismatch<>,poly_mismatch<Types...>>(
        p,first,end,pred);

      test_algorithms<std_equal<>,poly_equal<>,poly_equal<Types...>>(
        p,first,pred);

      test_algorithms<std_equal<>,poly_equal<>,poly_equal<Types...>>(
        p,first,end,pred);
    }

    test_algorithms<std_mismatch<>,poly_mismatch<>,poly_mismatch<Types...>>(
      p,v.end(),v.end(),pred);

    test_algorithms<std_equal<>,poly_equal<>,poly_equal<Types...>>(
      p,v.end(),v.end(),pred);
  }
  {
    using value_type=typename PolyCollection::value_type;
    using vector=std::vector<std::reference_wrapper<const value_type>>;

    vector v;
    for(const auto& x:p)v.push_back(x);
    auto w=v;
    std::mt19937 gen{73642};
    std::shuffle(w.begin(),w.end(),gen);
    v.insert(v.end(),w.begin(),w.end());
    auto pred=compose_all(to_int,std::equal_to<int>{});

    for(auto first=unwrap_iterator(v.begin()),
             end=unwrap_iterator(v.begin()+v.size()/2);first!=end;++first){
      test_algorithms_with_equality<
        std_is_permutation<>,
        poly_is_permutation<>,
        only_eq_comparable<poly_is_permutation,Types...>
      >(p,first);

      test_algorithms<
        std_is_permutation<>,
        poly_is_permutation<>,poly_is_permutation<Types...>
      >(p,first,pred);

      test_algorithms_with_equality<
        std_is_permutation<>,
        poly_is_permutation<>,
        only_eq_comparable<poly_is_permutation,Types...>
      >(p,first,end);

      test_algorithms<
        std_is_permutation<>,
        poly_is_permutation<>,poly_is_permutation<Types...>
      >(p,first,end,pred);
    }

    test_algorithms_with_equality<
      std_is_permutation<>,
      poly_is_permutation<>,
      only_eq_comparable<poly_is_permutation,Types...>
    >(p,unwrap_iterator(v.end()),unwrap_iterator(v.end()));

    test_algorithms<
      std_is_permutation<>,
      poly_is_permutation<>,poly_is_permutation<Types...>
    >(p,unwrap_iterator(v.end()),unwrap_iterator(v.end()),pred);
  }
  {
    /* search tested above */
  }
  {
    for(const auto&x: p){
      for(int n=0;n<3;++n){
        test_algorithms_with_equality<
          std_search_n<>,poly_search_n<>,
          only_eq_comparable<poly_search_n,Types...>
        >(p,n,x);
      }
    }
  }
  {
    for(int n=0;n<6;++n){
      test_algorithms<std_search_n<>,poly_search_n<>,poly_search_n<Types...>>(
        p,n,0,compose(to_int,[&](int x,int y){return x%(6-n)==y%(6-n);}));
    }
  }
  {
    test_copy_algorithms<std_copy<>,poly_copy<>,poly_copy<Types...>>(
      to_int,p);
  }
  {
    test_copy_n_algorithms<std_copy_n<>,poly_copy_n<>,poly_copy_n<Types...>>(
      to_int,p);
  }
  {
    auto always_true=compose(to_int,[](int){return true;});
    auto always_false=compose(to_int,[](int){return false;});
    auto pred=compose(to_int,[](int x){return x%2==0;});

    test_copy_algorithms<std_copy_if<>,poly_copy_if<>,poly_copy_if<Types...>>(
      to_int,p,always_true);
    test_copy_algorithms<std_copy_if<>,poly_copy_if<>,poly_copy_if<Types...>>(
      to_int,p,always_false);
    test_copy_algorithms<std_copy_if<>,poly_copy_if<>,poly_copy_if<Types...>>(
      to_int,p,pred);
  }
  {
    test_copy_algorithms<std_move<>,poly_move<>,poly_move<Types...>>(
      to_int,p); /* we're not checking std::move is properly used internally */
  }
  {
    auto f=compose(to_int,[](int x){return -x;});
    auto int_id=[](int x){return x;};

    test_copy_algorithms<
      std_transform<>,poly_transform<>,poly_transform<Types...>
    >(int_id,p,f);
  }
  {
    test_transform2_algorithms<
      std_transform<>,poly_transform<>,poly_transform<Types...>
    >(to_int,p);
  }
  {
    const auto& y=*p.begin();
    for(const auto& x:p){
      test_copy_algorithms_with_equality<
        std_replace_copy<>,poly_replace_copy<>,
        only_eq_comparable<poly_replace_copy,Types...>
      >(to_int,p,x,y);

      test_copy_algorithms_with_equality<
        std_remove_copy<>,poly_remove_copy<>,
        only_eq_comparable<poly_remove_copy,Types...>
      >(to_int,p,x);
    }
  }
  {
    auto  always_true=compose(to_int,[](int){return true;});
    auto  always_false=compose(to_int,[](int){return false;});
    auto  pred=compose(to_int,[](int x){return x%2==0;});
    auto& x=*p.begin();

    test_copy_algorithms<
      std_replace_copy_if<>,
      poly_replace_copy_if<>,poly_replace_copy_if<Types...>
    >(to_int,p,always_true,x);
    test_copy_algorithms<
      std_replace_copy_if<>,
      poly_replace_copy_if<>,poly_replace_copy_if<Types...>
    >(to_int,p,always_false,x);
    test_copy_algorithms<
      std_replace_copy_if<>,
      poly_replace_copy_if<>,poly_replace_copy_if<Types...>
    >(to_int,p,pred,x);

    test_copy_algorithms<
      std_remove_copy_if<>,
      poly_remove_copy_if<>,poly_remove_copy_if<Types...>
    >(to_int,p,always_true);
    test_copy_algorithms<
      std_remove_copy_if<>,
      poly_remove_copy_if<>,poly_remove_copy_if<Types...>
    >(to_int,p,always_false);
    test_copy_algorithms<
      std_remove_copy_if<>,
      poly_remove_copy_if<>,poly_remove_copy_if<Types...>
    >(to_int,p,pred);
  }
  {
    test_copy_algorithms_with_equality<
      std_unique_copy<>,poly_unique_copy<>,
      only_eq_comparable<poly_unique_copy,Types...>
    >(to_int,p);
  }
  {
    for(int n=0;n<6;++n){
      test_copy_algorithms<
        std_unique_copy<>,poly_unique_copy<>,poly_unique_copy<Types...>
      >(to_int,p,
        compose_all(to_int,[&](int x,int y){return x%(6-n)==y%(6-n);}));
    }
  }
  {
    test_rotate_copy_algorithms<
      std_rotate_copy<>,poly_rotate_copy<>,poly_rotate_copy<Types...>
    >(to_int,p);
  }
  {
    /* force_arg_copying used to avoid sharing mt19937's state */

    for(auto n=p.size()+1;n-->0;){
      test_copy_algorithms<
        force_arg_copying<std_sample<>>,
        force_arg_copying<poly_sample<>>,
        force_arg_copying<poly_sample<Types...>>
      >(to_int,p,n,std::mt19937{});
    }
  }
  {
    for(int n=0;n<6;++n){
      auto pred=compose(to_int,[&](int x){return x%(6-n)<=(6-n)/2;});

      test_algorithms<
        std_is_partitioned<>,
        poly_is_partitioned<>,poly_is_partitioned<Types...>
      >(p,pred);

      test_partition_copy_algorithms<
        std_partition_copy<>,
        poly_partition_copy<>,poly_partition_copy<Types...>
      >(to_int,p,pred);

      test_algorithms<
        std_partition_point<>,
        poly_partition_point<>,poly_partition_point<Types...>
      >(p,pred);
    }
  }
}

#endif
