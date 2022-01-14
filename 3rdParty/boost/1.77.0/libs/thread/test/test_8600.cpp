// Copyright (C) 2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <vector>
#include <utility>
#include <type_traits>
#if 1

struct B {
  int v;
  B(int i) : v(i)  {}
};

struct D: B {
  D(int i) : B(i)  {}
};

void fb(B const&) {}
void fd(D const&) {}

BOOST_STATIC_ASSERT(sizeof(B)==sizeof(D));

template <class T, class Allocator=std::allocator<T> >
class new_vector;
template <class T, class Allocator>
class new_vector : public std::vector<T,Allocator>
{
  typedef std::vector<T,Allocator> base_type;
public:
  new_vector() : base_type()  {}
  new_vector(unsigned s) : base_type(s)  {}
};

template <class Allocator >
class new_vector<bool, Allocator>
{
  //std::vector<char,typename Allocator::template rebind<char>::other > v;
  int i;
public:
};

template <class T, class A>
typename std::enable_if<!std::is_same<T, bool>::value,
  new_vector<T,A>&
>::type
new_vector_cast(std::vector<T,A> & v) {
  return reinterpret_cast<new_vector<T,A>&>(v);
}

BOOST_STATIC_ASSERT(sizeof(std::vector<int>)==sizeof(new_vector<int>));
BOOST_STATIC_ASSERT(sizeof(std::vector<bool>)!=sizeof(new_vector<bool>));

void fb(std::vector<int> const&) {}
void fd(new_vector<int> const&) {}

int main() {
  {
    std::vector<int> b(1);
    b[0] = 1;
    new_vector<int> d = new_vector_cast(b);
    BOOST_ASSERT(b[0] == d[0]);
  }
  {
    //std::vector<bool> b;
    //new_vector<bool> d = new_vector_cast(b); // compile fail
  }
  {
    std::vector<int> b(1);
    b[0] = 1;
    fd(new_vector_cast(b));
  }
  {
    new_vector<int> d(1);
    d[0] = 1;
    std::vector<int> b = d;
    BOOST_ASSERT(b[0] == d[0]);
  }
  {
    //new_vector<bool> d;
    //std::vector<bool> b = d; // compile fail
  }
  {
    new_vector<int> d(1);
    d[0] = 1;
    fd(d);
  }
  return 0;
}


#else
int main() {
  {
    B b(1);
    D d = reinterpret_cast<D&>(b);
    BOOST_ASSERT(b.v == d.v);
  }
  {
    B b(1);
    fd(reinterpret_cast<D&>(b));
  }
  {
    D d(2);
    B b = d;
    BOOST_ASSERT(b.v == d.v);
  }
  {
    D d(2);
    fd(d);
  }
  return 0;
}

#define BOOST_THREAD_VERSION 4

#include <iostream>
#include <boost/thread.hpp>

int calculate_the_answer_to_life_the_universe_and_everything()
{
return 42;
}

int main()
{
boost::packaged_task<int()> pt(calculate_the_answer_to_life_the_universe_and_everything);
boost::shared_future<int> fi1 = boost::shared_future<int>(pt.get_future());
boost::shared_future<int> fi2 = fi1;

boost::thread task(boost::move(pt)); // launch task on a thread

boost::wait_for_any(fi1, fi2);
std::cout << "Wait for any returned\n";
return (0);
}
#endif
