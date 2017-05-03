// (C) Copyright 2013 Ruslan Baratov
// Copyright (C) 2014 Vicente J. Botet Escriba
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See www.boost.org/libs/thread for documentation.

#define BOOST_THREAD_VERSION 4

#include <boost/detail/lightweight_test.hpp> // BOOST_TEST

#include <boost/thread/mutex.hpp>
#include <boost/thread/with_lock_guard.hpp>

class Foo {
 public:
  explicit Foo(int a) : a_(a) {
  }

  Foo(BOOST_RV_REF(Foo) foo) : a_(foo.a_) {
    BOOST_ASSERT(&foo != this);
    foo.a_ = 0;
  }

  Foo& operator=(BOOST_RV_REF(Foo) foo) {
    BOOST_ASSERT(&foo != this);
    a_ = foo.a_;
    foo.a_ = 0;
    return *this;
  }

  int get() const {
    return a_;
  }

 private:
  BOOST_MOVABLE_BUT_NOT_COPYABLE(Foo)

  int a_;
};

template <class T1, class T2>
bool func_with_2_arg(BOOST_FWD_REF(T1) arg_1, BOOST_FWD_REF(T2) arg_2) {
  BOOST_TEST(arg_1.get() == 3);
  BOOST_TEST(arg_2.get() == 767);
  return false;
}

void test_movable() {
  boost::mutex m;

  Foo foo_1(3);
  Foo foo_2(767);

  bool res = boost::with_lock_guard(
      m, &func_with_2_arg<Foo, Foo>, boost::move(foo_1), boost::move(foo_2)
  );
  BOOST_TEST(!res);
}

#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
void test_real_movable() {
  std::cout << "c++11 move emulated" << std::endl;
}
#else
// test real one
class Boo {
 public:
  Boo(int a) : a_(a) {
  }

  Boo(Boo&& boo) : a_(boo.a_) {
    BOOST_ASSERT(&boo != this);
    boo.a_ = 0;
  }

  int get() const {
    return a_;
  }

  BOOST_DELETED_FUNCTION(Boo(Boo&))
  BOOST_DELETED_FUNCTION(Boo& operator=(Boo&))
  BOOST_DELETED_FUNCTION(Boo& operator=(Boo&&))
 private:
  int a_;
};

void func_with_3_arg(Boo&& boo_1, Boo&& boo_2, Boo&& boo_3) {
  BOOST_TEST(boo_1.get() == 11);
  BOOST_TEST(boo_2.get() == 12);
  BOOST_TEST(boo_3.get() == 13);
}

void test_real_movable() {
  boost::mutex m;

  Boo boo_3(13);

  boost::with_lock_guard(
      m, func_with_3_arg, Boo(11), Boo(12), boost::move(boo_3)
  );
}
#endif

int main() {
  test_movable();
  test_real_movable();
  return boost::report_errors();
}
