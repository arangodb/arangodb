/* Boost.MultiIndex test for standard hash operations.
 *
 * Copyright 2003-2013 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_hash_ops.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <iterator>
#include <boost/detail/lightweight_test.hpp>
#include "pre_multi_index.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <vector>

#include <iostream>

using namespace boost::multi_index;

template<typename HashedContainer>
void check_load_factor(const HashedContainer& hc)
{
  float lf=(float)hc.size()/hc.bucket_count();
  BOOST_TEST(lf<=hc.load_factor()+1.E-6);
  BOOST_TEST(lf>=hc.load_factor()-1.E-6);
  BOOST_TEST(lf<=hc.max_load_factor()+1.E-6);
}

typedef multi_index_container<
  int,
  indexed_by<
    hashed_unique<identity<int> >
  >
> hash_container;

void test_hash_ops()
{
  hash_container hc;

  BOOST_TEST(hc.max_load_factor()==1.0f);
  BOOST_TEST(hc.bucket_count()<=hc.max_bucket_count());

  hc.insert(1000);
  hash_container::size_type buc=hc.bucket(1000);
  hash_container::local_iterator it0=hc.begin(buc);
  hash_container::local_iterator it1=hc.end(buc);
  BOOST_TEST(
    (hash_container::size_type)std::distance(it0,it1)==hc.bucket_size(buc)&&
    hc.bucket_size(buc)==1&&*it0==1000);

  hc.clear();

  for(hash_container::size_type s=2*hc.bucket_count();s--;){
    hc.insert((int)s);
  }
  check_load_factor(hc);

  hc.max_load_factor(0.5f);
  BOOST_TEST(hc.max_load_factor()==0.5f);
  hc.insert(-1);
  check_load_factor(hc);

  hc.rehash(1);
  BOOST_TEST(hc.bucket_count()>=1);
  check_load_factor(hc);

  hc.max_load_factor(0.25f);
  hc.rehash(1);
  BOOST_TEST(hc.bucket_count()>=1);
  check_load_factor(hc);

  hash_container::size_type bc=4*hc.bucket_count();
  hc.max_load_factor(0.125f);
  hc.rehash(bc);
  BOOST_TEST(hc.bucket_count()>=bc);
  check_load_factor(hc);

  bc=2*hc.bucket_count();
  hc.rehash(bc);
  BOOST_TEST(hc.bucket_count()>=bc);
  check_load_factor(hc);

  hc.clear();
  hc.insert(0);
  hc.rehash(1);
  BOOST_TEST(hc.bucket_count()>=1);
  check_load_factor(hc);

  hash_container hc2;
  hc2.insert(0);
  hc2.max_load_factor(0.5f/hc2.bucket_count());
  BOOST_TEST(hc2.load_factor()>hc2.max_load_factor());
  hc2.reserve(1);
  BOOST_TEST(hc2.load_factor()<hc2.max_load_factor());

  hash_container hc3;
  hc3.clear();
  hc3.max_load_factor(1.0f);
  hc3.reserve(1000);
  hash_container::size_type bc3=hc3.bucket_count();
  BOOST_TEST(bc3>=1000);
  std::vector<int> v;
  for(unsigned int n=0;n<bc3;++n)v.push_back(n);
  hc3.insert(v.begin(),v.end());
  BOOST_TEST(hc3.bucket_count()==bc3); /* LWG issue 2156 */
  hc3.max_load_factor(0.25f);
  hc3.reserve(100);
  BOOST_TEST(hc3.bucket_count()>bc3);
  bc3=hc3.bucket_count();
  hc3.reserve((hash_container::size_type)(3.0f*hc3.max_load_factor()*bc3));
  BOOST_TEST(hc3.bucket_count()>bc3);
}
