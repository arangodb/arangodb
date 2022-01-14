//  (C) Copyright Raffi Enficiaud 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// Tests the variadic sample element support and the respect of move semantics
/// for the datasets definitions
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE test movable return type on datasets test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <boost/test/data/monomorphic/singleton.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/for_each_sample.hpp>


namespace utf=boost::unit_test;
namespace bdata=utf::data;

#include <vector>
#include <list>


class non_copyable_type
{
public:
  static int nb_rvalue_construct;
  static int nb_rvalue_assignment;
  static int nb_destructs;

  non_copyable_type(const non_copyable_type&) = delete;
  non_copyable_type& operator=(const non_copyable_type&) = delete;

  ~non_copyable_type() {
    nb_destructs++;
  }

  non_copyable_type(non_copyable_type&& rhs)
  {
    value_ = rhs.value_;
    rhs.value_ = -1;
    nb_rvalue_construct++;
  }
  non_copyable_type& operator=(non_copyable_type&& rhs) {
    value_ = rhs.value_;
    rhs.value_ = -1;
    nb_rvalue_assignment++;
    return *this;
  }

  explicit non_copyable_type(const int value)
  {
    value_ = value;
  }

  friend std::ostream& operator<<(std::ostream& ost, const non_copyable_type& rhs)
  {
    ost << "non_copyable_type: " << rhs.value_ << std::endl;
    return ost;
  }

  int get() const {
    return value_;
  }

private:
  int value_;
};

int non_copyable_type::nb_rvalue_construct = 0;
int non_copyable_type::nb_rvalue_assignment = 0;
int non_copyable_type::nb_destructs = 0;

// Dataset generating a Fibonacci sequence
// The return type is either a regular int or a non-movable type
template <class return_t = int>
class fibonacci_dataset {
public:
    enum { arity = 1 };

    struct iterator {

        iterator() : a(1), b(1) {}

        return_t operator*() const   { return return_t(b); }
        void operator++()
        {
            a = a + b;
            std::swap(a, b);
        }
    private:
        int a;
        int b; // b is the output
    };

    fibonacci_dataset()             {}

    // size is infinite
    bdata::size_t   size() const    { return bdata::BOOST_TEST_DS_INFINITE_SIZE; }

    // iterator
    iterator        begin() const   { return iterator(); }
};

namespace boost { namespace unit_test { namespace data { namespace monomorphic {
  // registering fibonacci_dataset as a proper dataset
  template <>
  struct is_dataset< fibonacci_dataset<int> > : boost::mpl::true_ {};

  template <>
  struct is_dataset< fibonacci_dataset<non_copyable_type> > : boost::mpl::true_ {};
}}}}

// Creating a test-driven dataset
BOOST_DATA_TEST_CASE(
    test1,
    fibonacci_dataset<int>() ^ bdata::make( { 1, 2, 3, 5, 8, 13, 21, 34, 55 } ),
    fib_sample, exp)
{
      BOOST_TEST(fib_sample == exp);
}

BOOST_DATA_TEST_CASE(
    test2,
    fibonacci_dataset<non_copyable_type>() ^ bdata::make( { 1, 2, 3, 5, 8, 13, 21, 34, 55 } ),
    fib_sample, exp)
{
      BOOST_TEST(fib_sample.get() == exp);
}

//____________________________________________________________________________//

// EOF
