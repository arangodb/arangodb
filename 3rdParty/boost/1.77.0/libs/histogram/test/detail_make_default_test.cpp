// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/make_default.hpp>
#include <new>
#include <vector>

using namespace boost::histogram::detail;

template <class T>
struct allocator_with_state {
  using value_type = T;

  allocator_with_state(int s = 0) : state(s) {}

  template <class U>
  allocator_with_state(const allocator_with_state<U>& o) : state(o.state) {}

  value_type* allocate(std::size_t n) {
    return static_cast<value_type*>(::operator new(n * sizeof(T)));
  }
  void deallocate(value_type* ptr, std::size_t) {
    ::operator delete(static_cast<void*>(ptr));
  }

  template <class U>
  bool operator==(const allocator_with_state<U>&) const {
    return true;
  }

  template <class U>
  bool operator!=(const allocator_with_state<U>&) const {
    return false;
  }

  int state;
};

int main() {

  using V = std::vector<int, allocator_with_state<int>>;
  V a(3, 0, allocator_with_state<int>{42});
  V b = make_default(a);
  V c;
  BOOST_TEST_EQ(a.size(), 3);
  BOOST_TEST_EQ(b.size(), 0);
  BOOST_TEST_EQ(c.size(), 0);
  BOOST_TEST_EQ(a.get_allocator().state, 42);
  BOOST_TEST_EQ(b.get_allocator().state, 42);
  BOOST_TEST_EQ(c.get_allocator().state, 0);

  return boost::report_errors();
}
