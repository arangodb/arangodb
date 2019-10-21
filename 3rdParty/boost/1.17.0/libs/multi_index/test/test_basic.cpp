/* Boost.MultiIndex basic test.
 *
 * Copyright 2003-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_basic.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <algorithm>
#include <vector>
#include "pre_multi_index.hpp"
#include "employee.hpp"
#include <boost/detail/lightweight_test.hpp>

using namespace boost::multi_index;

struct less_by_employee_age
{
  bool operator()(const employee& e1,const employee& e2)const
  {
    return e1.age<e2.age;
  }
};

struct no_addressof_type
{
  no_addressof_type(int n):n(n){}

  void operator&()const{}

  int n;
};

bool operator==(const no_addressof_type& x,const no_addressof_type& y)
{
  return x.n==y.n;
}

bool operator<(const no_addressof_type& x,const no_addressof_type& y)
{
  return x.n<y.n;
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
namespace boost{
#endif

inline std::size_t hash_value(const no_addressof_type& x)
{
  boost::hash<int> h;
  return h(x.n);
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
} /* namespace boost */
#endif

void test_basic()
{
  employee_set          es;
  std::vector<employee> v;

#if defined(BOOST_NO_MEMBER_TEMPLATES)
  employee_set_by_name& i1=get<by_name>(es);
#else
  employee_set_by_name& i1=es.get<by_name>();
#endif

  const employee_set_by_age& i2=get<2>(es);
  employee_set_as_inserted&  i3=get<3>(es);
  employee_set_by_ssn&       i4=get<ssn>(es);
  employee_set_randomly&     i5=get<randomly>(es);

  es.insert(employee(0,"Joe",31,1123));
  es.insert(employee(5,"Anna",41,1123));      /* clash*/
  i1.insert(employee(1,"Robert",27,5601));
  es.insert(employee(2,"John",40,7889));
  i3.push_back(employee(3,"Albert",20,9012));
  i4.insert(employee(4,"John",57,1002));
  i5.push_back(employee(0,"Andrew",60,2302)); /* clash */

  v.push_back(employee(0,"Joe",31,1123));
  v.push_back(employee(1,"Robert",27,5601));
  v.push_back(employee(2,"John",40,7889));
  v.push_back(employee(3,"Albert",20,9012));
  v.push_back(employee(4,"John",57,1002));

  {
    /* by insertion order */

    BOOST_TEST(std::equal(i3.begin(),i3.end(),v.begin()));
    BOOST_TEST(std::equal(i5.begin(),i5.end(),v.begin()));
  }

  {
    /* by id */

    std::sort(v.begin(),v.end());
    BOOST_TEST(std::equal(es.begin(),es.end(),v.begin()));
  }

  {
    /* by age */

    std::sort(v.begin(),v.end(),less_by_employee_age());
    BOOST_TEST(std::equal(i2.begin(),i2.end(),v.begin()));
  }

  {
    /* testcase for https://svn.boost.org/trac10/ticket/13307 */

    typedef multi_index_container<
      no_addressof_type,
      indexed_by<
        random_access<>,
        ordered_non_unique<identity<no_addressof_type> >,
        sequenced<>,
        hashed_non_unique<identity<no_addressof_type> >
      >
    > multi_index_t;

    multi_index_t        c;
    const multi_index_t& cc=c;
    no_addressof_type    x(0);
    int                  a[]={1,2};
    int                  b[]={6,7};
    c.push_back(x);
    c.insert(c.end(),a,a+2);
    c.push_back(no_addressof_type(3));
    c.emplace_back(4);
    c.get<1>().emplace_hint(c.get<1>().begin(),5);
    c.get<1>().insert(b,b+2);
    (void)c.begin()->n;
    (void)c.get<1>().begin()->n;
    (void)c.get<2>().begin()->n;
    (void)c.get<3>().begin()->n;
    (void)c.get<3>().begin(0)->n;
    (void)c.iterator_to(c.front());
    (void)cc.iterator_to(c.front());
    (void)c.get<1>().iterator_to(c.front());
    (void)cc.get<1>().iterator_to(c.front());
    (void)c.get<2>().iterator_to(c.front());
    (void)cc.get<2>().iterator_to(c.front());
    (void)c.get<3>().iterator_to(c.front());
    (void)cc.get<3>().iterator_to(c.front());
    (void)c.get<3>().local_iterator_to(c.front());
    (void)cc.get<3>().local_iterator_to(c.front());
    multi_index_t c2=c;(void)c2;
    c.erase(c.begin());
    c.erase(c.begin(),c.end());
  }

}
