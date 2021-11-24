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
#include <boost/bind/bind.hpp>

class Foo {
 public:
  Foo(int value): value_(value) {
  }

  int func(int a, int b) const {
    BOOST_TEST(a == 1);
    BOOST_TEST(b == 31);
    return a + b + value_;
  }

  int func_ref(int& a) const {
    a = 133;
    return 36;
  }

  void func_ref(int& a, int& b, int* c) const {
    BOOST_TEST(value_ == 3);
    a = 567;
    b = 897;
    *c = 345;
  }

 private:
  int value_;
};

void test_bind() {
  boost::mutex m;

  Foo foo(2);

  int res_bind = boost::with_lock_guard(
      m,
      boost::bind(&Foo::func, foo, 1, 31)
  );
  BOOST_TEST(res_bind == 34);

  int a = 0;
  int res_bind_ref = boost::with_lock_guard(
      m,
      boost::bind(&Foo::func_ref, foo, boost::ref(a))
  );
  BOOST_TEST(res_bind_ref == 36);
  BOOST_TEST(a == 133);

  a = 0;
  int b = 0;
  int c = 0;
  Foo boo(3);
  boost::with_lock_guard(
      m, boost::bind(&Foo::func_ref, boo, boost::ref(a), boost::ref(b), &c)
  );
  BOOST_TEST(a == 567);
  BOOST_TEST(b == 897);
  BOOST_TEST(c == 345);
}

#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
void test_bind_non_const() {
  std::cout << "c++11 variadic templates disabled" << std::endl;
}
#else

// calling non-const bind methods supported only with c++11 variadic templates
class Boo {
 public:
  Boo(int value): value_(value) {
  }

  int func(int a, int b) {
    BOOST_TEST(a == 7);
    BOOST_TEST(b == 3);
    return a - b + value_;
  }

  int func_ref(int& a) {
    a = 598;
    return 23;
  }

  void func_ref(int& a, int& b, int* c) {
    BOOST_TEST(value_ == 67);
    a = 111;
    b = 222;
    *c = 333;
  }

 private:
  int value_;
};

void test_bind_non_const() {
  boost::mutex m;

  Boo boo(20);

  int res_bind = boost::with_lock_guard(
      m,
      boost::bind(&Boo::func, boo, 7, 3)
  );
  BOOST_TEST(res_bind == 24);

  int a = 0;
  int res_bind_ref = boost::with_lock_guard(
      m,
      boost::bind(&Boo::func_ref, boo, boost::ref(a))
  );
  BOOST_TEST(res_bind_ref == 23);
  BOOST_TEST(a == 598);

  a = 0;
  int b = 0;
  int c = 0;
  Boo foo(67);
  boost::with_lock_guard(
      m, boost::bind(&Boo::func_ref, foo, boost::ref(a), boost::ref(b), &c)
  );
  BOOST_TEST(a == 111);
  BOOST_TEST(b == 222);
  BOOST_TEST(c == 333);
}
#endif

int main() {
  test_bind();
  test_bind_non_const();
  return boost::report_errors();
}
