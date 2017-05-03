// (C) Copyright 2013 Ruslan Baratov
// (C) Copyright 2013 Ruslan Baratov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See www.boost.org/libs/thread for documentation.

#define BOOST_THREAD_VERSION 4

#include <boost/detail/lightweight_test.hpp> // BOOST_TEST

#include <boost/thread/mutex.hpp>
#include <boost/thread/with_lock_guard.hpp>
#include <boost/ref.hpp>

void func_with_0_arg() {
}

void func_with_1_arg(int arg_1) {
  BOOST_TEST(arg_1 == 3);
}

bool func_with_2_arg(int arg_1, bool arg_2) {
  BOOST_TEST(arg_1 == 3);
  BOOST_TEST(arg_2 == true);
  return !arg_2;
}

int func_with_3_arg(int arg_1, bool arg_2, const char* arg_3) {
  BOOST_TEST(arg_1 == 13);
  BOOST_TEST(arg_2 == false);
  BOOST_TEST(std::string(arg_3) == "message for func with 3 arg");
  return 12;
}

const char* func_with_4_arg(int arg_1, bool arg_2, int* arg_3, int& arg_4) {
  BOOST_TEST(arg_1 == 23);
  BOOST_TEST(arg_2 == false);
  *arg_3 = 128;
  arg_4 = 456;
  return "hello";
}

void test_simple() {
  boost::mutex m;

  // #0
  boost::with_lock_guard(m, func_with_0_arg);

  // #1
  boost::with_lock_guard(m, func_with_1_arg, 3);

  // #2
  bool res2 = boost::with_lock_guard(m, func_with_2_arg, 3, true);
  BOOST_TEST(res2 == false);

  // #3
  int arg1 = 13;
  const char* mes = "message for func with 3 arg";
  int res3 = boost::with_lock_guard(m, func_with_3_arg, arg1, false, mes);
  BOOST_TEST(res3 == 12);

  // #4
  int arg3 = 0;
  int arg4 = 0;
  const char* res4 = boost::with_lock_guard(
      m,
      func_with_4_arg,
      23,
      false,
      &arg3,
      boost::ref(arg4)
  );
  BOOST_TEST(arg3 == 128);
  BOOST_TEST(arg4 == 456);
  BOOST_TEST(std::string(res4) == "hello");
}

#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
void test_variadic_templates() {
  std::cout << "C++11 variadic templates disabled" << std::endl;
}
#else
int func_with_5_args(int a1, char a2, int& a3, bool* a4, bool a5) {
  BOOST_TEST(a1 == 12);
  BOOST_TEST(a2 == 'x');
  BOOST_TEST(a5 == false);
  a3 = 135;
  *a4 = false;
  return 45;
}

int func_with_6_args(int a1, char a2, int& a3, bool* a4, int&& a5, bool a6) {
  BOOST_TEST(a1 == 12);
  BOOST_TEST(a2 == 'N');
  BOOST_TEST(a5 == 2 || a5 == 13);
  BOOST_TEST(a6 == false);
  a3 = 200;
  *a4 = true;
  return 888;
}

void test_variadic_templates() {
  boost::mutex m;

  int a3 = 0;
  bool a4 = true;
  int res5 = boost::with_lock_guard(
      m, func_with_5_args, 12, 'x', a3, &a4, false
  );
  BOOST_TEST(a3 == 135);
  BOOST_TEST(a4 == false);
  BOOST_TEST(res5 == 45);

  int res6 = boost::with_lock_guard(
      m, func_with_6_args, 12, 'N', a3, &a4, 2, false
  );
  BOOST_TEST(a3 == 200);
  BOOST_TEST(a4 == true);
  BOOST_TEST(res6 == 888);

  a3 = 0;
  a4 = false;
  int a5 = 13;
  int res6_move = boost::with_lock_guard(
      m, func_with_6_args, 12, 'N', a3, &a4, boost::move(a5), false
  );
  BOOST_TEST(a3 == 200);
  BOOST_TEST(a4 == true);
  BOOST_TEST_EQ(res6_move, 888);
}
#endif

int main() {
  test_simple();
  test_variadic_templates();
  return boost::report_errors();
}
