/* Boost.MultiIndex test for rank operations.
 *
 * Copyright 2003-2015 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_rank_ops.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <algorithm>
#include <iterator>
#include <set>
#include <boost/detail/lightweight_test.hpp>
#include "pre_multi_index.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/ranked_index.hpp>

using namespace boost::multi_index;

template<
  typename Sequence1,typename Iterator2,typename Sequence2
>
bool same_position(
  std::size_t n1,const Sequence1& s1,Iterator2 it2,const Sequence2& s2)
{
  typedef typename Sequence1::const_iterator const_iterator1;
  typedef typename Sequence2::const_iterator const_iterator2;

  const_iterator1 cit1=s1.begin();
  std::advance(cit1,n1);
  const_iterator2 cit2=it2;
  return std::distance(s1.begin(),cit1)==std::distance(s2.begin(),cit2);
}

struct less_equal_than
{
  less_equal_than(int n_):n(n_){}
  bool operator()(int x)const{return x<=n;}
  int n;
};

struct greater_equal_than
{
  greater_equal_than(int n_):n(n_){}
  bool operator()(int x)const{return x>=n;}
  int n;
};

template<typename Sequence>
static void local_test_rank_ops()
{
  int                data[]={2,2,1,5,6,7,9,10,9,6,9,6,9};
  Sequence           s(data,data+sizeof(data)/sizeof(data[0]));
  std::multiset<int> ss(s.begin(),s.end());

  typedef typename Sequence::iterator iterator;

  iterator it=s.begin();
  for(std::size_t n=0;n<=s.size()+1;++n){
    BOOST_TEST(s.nth(n)==it);
    BOOST_TEST(s.rank(it)==(std::min)(s.size(),n));
    if(it!=s.end())++it;
  }

  std::pair<std::size_t,std::size_t> p1;
  std::pair<iterator,iterator>       p2;

  p1=s.range_rank(unbounded,unbounded);
  p2=s.range(unbounded,unbounded);
  BOOST_TEST(same_position(p1.first,s,p2.first,s));
  BOOST_TEST(same_position(p1.second,s,p2.second,s));

  for(int i=0;i<12;++i){
    std::size_t pos=s.find_rank(i);
    BOOST_TEST((pos==s.size()&&ss.find(i)==ss.end())||(*s.nth(pos)==i));
    BOOST_TEST(same_position(s.lower_bound_rank(i),s,ss.lower_bound(i),ss));
    BOOST_TEST(same_position(s.upper_bound_rank(i),s,ss.upper_bound(i),ss));
    std::pair<std::size_t,std::size_t> posp=s.equal_range_rank(i);
    BOOST_TEST(same_position(posp.first,s,ss.lower_bound(i),ss));
    BOOST_TEST(same_position(posp.second,s,ss.upper_bound(i),ss));

    p1=s.range_rank(greater_equal_than(i),unbounded);
    p2=s.range(greater_equal_than(i),unbounded);
    BOOST_TEST(same_position(p1.first,s,p2.first,s));
    BOOST_TEST(same_position(p1.second,s,p2.second,s));
    p1=s.range_rank(unbounded,less_equal_than(i));
    p2=s.range(unbounded,less_equal_than(i));
    BOOST_TEST(same_position(p1.first,s,p2.first,s));
    BOOST_TEST(same_position(p1.second,s,p2.second,s));

    for(int j=0;j<12;++j){
      p1=s.range_rank(greater_equal_than(i),less_equal_than(j));
      p2=s.range(greater_equal_than(i),less_equal_than(j));
      BOOST_TEST(same_position(p1.first,s,p2.first,s));
      BOOST_TEST(same_position(p1.second,s,p2.second,s));
    }
  }

  Sequence se; /* empty */
  BOOST_TEST(se.nth(0)==se.end());
  BOOST_TEST(se.nth(1)==se.end());
  BOOST_TEST(se.rank(se.end())==0);
  BOOST_TEST(se.find_rank(0)==0);
  BOOST_TEST(se.lower_bound_rank(0)==0);
  BOOST_TEST(se.upper_bound_rank(0)==0);
  p1=se.equal_range_rank(0);
  BOOST_TEST(p1.first==0&&p1.second==0);
  p1=se.range_rank(unbounded,unbounded);
  BOOST_TEST(p1.first==0&&p1.second==0);
  p1=se.range_rank(greater_equal_than(0),unbounded);
  BOOST_TEST(p1.first==0&&p1.second==0);
  p1=se.range_rank(unbounded,less_equal_than(0));
  BOOST_TEST(p1.first==0&&p1.second==0);
  p1=se.range_rank(greater_equal_than(0),less_equal_than(0));
  BOOST_TEST(p1.first==0&&p1.second==0);
}

void test_rank_ops()
{
  typedef multi_index_container<
    int,
    indexed_by<
      ranked_unique<identity<int> >
    >
  > ranked_set;
  
  local_test_rank_ops<ranked_set>();

  typedef multi_index_container<
    int,
    indexed_by<
      ranked_non_unique<identity<int> >
    >
  > ranked_multiset;
  
  local_test_rank_ops<ranked_multiset>();
}
