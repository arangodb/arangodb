/* Boost.MultiIndex test for standard set operations.
 *
 * Copyright 2003-2021 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_set_ops.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <algorithm>
#include <vector>
#include "pre_multi_index.hpp"
#include "employee.hpp"
#include <boost/detail/lightweight_test.hpp>

using namespace boost::multi_index;

struct type1{};

struct type2
{
private:
  operator type1()const{return type1();}
};

struct type3
{
  operator type1()const{return type1();}
};

struct less_type12
{
  bool operator()(type1,type1)const{return false;}
  bool operator()(type1,type2)const{return false;}
  bool operator()(type2,type1)const{return false;}
};

bool less_type1_f(type1,type1){return false;}

struct hash_type12
{
  std::size_t operator()(type1)const{return 0;}
  std::size_t operator()(type2)const{return 0;}
};

struct eq_type12
{
  bool operator()(type1,type1)const{return true;}
  bool operator()(type1,type2)const{return true;}
  bool operator()(type2,type1)const{return true;}
};

void test_set_ops()
{
  employee_set               es;
  employee_set_by_name&      i1=get<by_name>(es);
  const employee_set_by_age& i2=get<age>(es);
  employee_set_by_ssn&       i4=get<ssn>(es);

  es.insert(employee(0,"Joe",31,1123));
  es.insert(employee(1,"Robert",27,5601));
  es.insert(employee(2,"John",40,7889));
  es.insert(employee(3,"Albert",20,9012));
  es.insert(employee(4,"John",57,1002));

  BOOST_TEST(i1.find("John")->name=="John");
  BOOST_TEST(i2.find(41)==i2.end());
  BOOST_TEST(i4.find(5601)->name=="Robert");

  BOOST_TEST(i1.count("John")==2);
  BOOST_TEST(es.count(employee(10,"",-1,0))==0);
  BOOST_TEST(i4.count(7881)==0);

  BOOST_TEST(i1.contains("John"));
  BOOST_TEST(!es.contains(employee(10,"",-1,0)));
  BOOST_TEST(!i4.contains(7881));

  BOOST_TEST(
    std::distance(
      i2.lower_bound(31),
      i2.upper_bound(60))==3);

  std::pair<employee_set_by_name::iterator,employee_set_by_name::iterator> p=
    i1.equal_range("John");
  BOOST_TEST(std::distance(p.first,p.second)==2);

  p=i1.equal_range("Serena");
  BOOST_TEST(p.first==i1.end()&&p.second==i1.end());

  std::pair<employee_set_by_age::iterator,employee_set_by_age::iterator> p2=
    i2.equal_range(30);
  BOOST_TEST(p2.first==p2.second&&p2.first->age==31);

  /* check promotion detection plays nice with private conversion */

  multi_index_container<
    type1,
    indexed_by<
      ordered_unique<identity<type1>,less_type12>,
      hashed_unique<identity<type1>,hash_type12,eq_type12>
    >
  > c;
  c.insert(type1());

  BOOST_TEST(c.find(type2())==c.begin());
  BOOST_TEST(c.count(type2())==1);
  BOOST_TEST(c.contains(type2()));
  BOOST_TEST(c.lower_bound(type2())==c.begin());
  BOOST_TEST(c.upper_bound(type2())==c.end());
  BOOST_TEST(c.equal_range(type2())==std::make_pair(c.begin(),c.end()));

  BOOST_TEST(c.get<1>().find(type2())==c.get<1>().begin());
  BOOST_TEST(c.get<1>().count(type2())==1);
  BOOST_TEST(c.get<1>().contains(type2()));
  BOOST_TEST(c.get<1>().equal_range(type2())==
             std::make_pair(c.get<1>().begin(),c.get<1>().end()));

  /* check promotion detection does not break with functions */

  multi_index_container<
    type1,
    indexed_by<
      ordered_unique<identity<type1>,bool(*)(type1,type1)>
    >
  > c2(boost::make_tuple(boost::make_tuple(identity<type1>(),&less_type1_f)));
  c2.insert(type1());

  BOOST_TEST(c2.find(type3())==c2.begin());
}
