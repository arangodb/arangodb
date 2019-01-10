// Copyright (C) 2003, 2008 Fernando Luis Cacciola Carballal.
// Copyright (C) 2015 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  fernando_cacciola@hotmail.com
//
// Revisions:
// 12 May 2008 (added more swap tests)
//

#include "boost/optional/optional.hpp"
#include "boost/utility/in_place_factory.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"

#if __cplusplus < 201103L
#include <algorithm>
#else
#include <utility>
#endif

using boost::optional;
using boost::none;

#define ARG(T) (static_cast< T const* >(0))

namespace optional_swap_test
{
  class default_ctor_exception : public std::exception {} ;
  class copy_ctor_exception : public std::exception {} ;
  class assignment_exception : public std::exception {} ;

  //
  // Base class for swap test classes.  Its assignment should not be called, when swapping
  // optional<T> objects.  (The default std::swap would do so.)
  //
  class base_class_with_forbidden_assignment
  {
  public:
    base_class_with_forbidden_assignment & operator=(const base_class_with_forbidden_assignment &)
    {
      BOOST_TEST(!"The assignment should not be used while swapping!");
      throw assignment_exception();
    }

    virtual ~base_class_with_forbidden_assignment() {}
  };

  //
  // Class without default constructor
  //
  class class_without_default_ctor : public base_class_with_forbidden_assignment
  {
  public:
    char data;
    explicit class_without_default_ctor(char arg) : data(arg) {}
  };

  //
  // Class whose default constructor should not be used by optional::swap!
  //
  class class_whose_default_ctor_should_not_be_used : public base_class_with_forbidden_assignment
  {
  public:
    char data;
    explicit class_whose_default_ctor_should_not_be_used(char arg) : data(arg) {}

    class_whose_default_ctor_should_not_be_used()
    {
      BOOST_TEST(!"This default constructor should not be used while swapping!");
      throw default_ctor_exception();
    }
  };

  //
  // Class whose default constructor should be used by optional::swap.
  // Its copy constructor should be avoided!
  //
  class class_whose_default_ctor_should_be_used : public base_class_with_forbidden_assignment
  {
  public:
    char data;
    explicit class_whose_default_ctor_should_be_used(char arg) : data(arg) { }

    class_whose_default_ctor_should_be_used() : data('\0') { }

    class_whose_default_ctor_should_be_used(const class_whose_default_ctor_should_be_used &)
    {
      BOOST_TEST(!"This copy constructor should not be used while swapping!");
      throw copy_ctor_exception();
    }
  };

  //
  // Class template whose default constructor should be used by optional::swap.
  // Its copy constructor should be avoided!
  //
  template <class T>
  class template_whose_default_ctor_should_be_used : public base_class_with_forbidden_assignment
  {
  public:
    T data;
    explicit template_whose_default_ctor_should_be_used(T arg) : data(arg) { }

    template_whose_default_ctor_should_be_used() : data('\0') { }

    template_whose_default_ctor_should_be_used(const template_whose_default_ctor_should_be_used &)
    {
      BOOST_TEST(!"This copy constructor should not be used while swapping!");
      throw copy_ctor_exception();
    }
  };

  //
  // Class whose explicit constructor should be used by optional::swap.
  // Its other constructors should be avoided!
  //
  class class_whose_explicit_ctor_should_be_used : public base_class_with_forbidden_assignment
  {
  public:
    char data;
    explicit class_whose_explicit_ctor_should_be_used(char arg) : data(arg) { }

    class_whose_explicit_ctor_should_be_used()
    {
      BOOST_TEST(!"This default constructor should not be used while swapping!");
      throw default_ctor_exception();
    }

    class_whose_explicit_ctor_should_be_used(const class_whose_explicit_ctor_should_be_used &)
    {
      BOOST_TEST(!"This copy constructor should not be used while swapping!");
      throw copy_ctor_exception();
    }
  };

  void swap(class_whose_default_ctor_should_not_be_used & lhs, class_whose_default_ctor_should_not_be_used & rhs)
  {
    std::swap(lhs.data, rhs.data);
  }

  void swap(class_whose_default_ctor_should_be_used & lhs, class_whose_default_ctor_should_be_used & rhs)
  {
    std::swap(lhs.data, rhs.data);
  }

  void swap(class_without_default_ctor & lhs, class_without_default_ctor & rhs)
  {
    std::swap(lhs.data, rhs.data);
  }

  void swap(class_whose_explicit_ctor_should_be_used & lhs, class_whose_explicit_ctor_should_be_used & rhs)
  {
    std::swap(lhs.data, rhs.data);
  }

  template <class T>
  void swap(template_whose_default_ctor_should_be_used<T> & lhs, template_whose_default_ctor_should_be_used<T> & rhs)
  {
    std::swap(lhs.data, rhs.data);
  }

  //
  // optional<T>::swap should be customized when neither the copy constructor
  // nor the default constructor of T are supposed to be used when swapping, e.g.,
  // for the following type T = class_whose_explicit_ctor_should_be_used.
  //
  void swap(boost::optional<class_whose_explicit_ctor_should_be_used> & x, boost::optional<class_whose_explicit_ctor_should_be_used> & y)
  {
    bool hasX(x);
    bool hasY(y);

    if ( !hasX && !hasY )
     return;

    if( !hasX )
       x = boost::in_place('\0');
    else if ( !hasY )
       y = boost::in_place('\0');

    optional_swap_test::swap(*x,*y);

     if( !hasX )
         y = boost::none ;
     else if( !hasY )
         x = boost::none ;
  }


} // End of namespace optional_swap_test.


namespace boost {

//
// Compile time tweaking on whether or not swap should use the default constructor:
//

template <> struct optional_swap_should_use_default_constructor<
  optional_swap_test::class_whose_default_ctor_should_be_used> : true_type {} ;

template <> struct optional_swap_should_use_default_constructor<
  optional_swap_test::class_whose_default_ctor_should_not_be_used> : false_type {} ;

template <class T> struct optional_swap_should_use_default_constructor<
  optional_swap_test::template_whose_default_ctor_should_be_used<T> > : true_type {} ;


//
// Specialization of boost::swap:
//
template <>
void swap(optional<optional_swap_test::class_whose_explicit_ctor_should_be_used> & x, optional<optional_swap_test::class_whose_explicit_ctor_should_be_used> & y)
{
  optional_swap_test::swap(x, y);
}

} // namespace boost


namespace std {

//
// Specializations of std::swap:
//

template <>
void swap(optional_swap_test::class_whose_default_ctor_should_be_used & x, optional_swap_test::class_whose_default_ctor_should_be_used & y)
{
  optional_swap_test::swap(x, y);
}

template <>
void swap(optional_swap_test::class_whose_default_ctor_should_not_be_used & x, optional_swap_test::class_whose_default_ctor_should_not_be_used & y)
{
  optional_swap_test::swap(x, y);
}

template <>
void swap(optional_swap_test::class_without_default_ctor & x, optional_swap_test::class_without_default_ctor & y)
{
  optional_swap_test::swap(x, y);
}

template <>
void swap(optional_swap_test::class_whose_explicit_ctor_should_be_used & x, optional_swap_test::class_whose_explicit_ctor_should_be_used & y)
{
  optional_swap_test::swap(x, y);
}

} // namespace std


//
// Tests whether the swap function works properly for optional<T>.
// Assumes that T has one data member, of type char.
// Returns true iff the test is passed.
//
template <class T>
void test_swap_function( T const* )
{
  try
  {
    optional<T> obj1;
    optional<T> obj2('a');

    // Self-swap should not have any effect.
    swap(obj1, obj1);
    swap(obj2, obj2);
    BOOST_TEST(!obj1);
    BOOST_TEST(!!obj2 && obj2->data == 'a');

    // Call non-member swap.
    swap(obj1, obj2);

    // Test if obj1 and obj2 are really swapped.
    BOOST_TEST(!!obj1 && obj1->data == 'a');
    BOOST_TEST(!obj2);

    // Call non-member swap one more time.
    swap(obj1, obj2);

    // Test if obj1 and obj2 are swapped back.
    BOOST_TEST(!obj1);
    BOOST_TEST(!!obj2 && obj2->data == 'a');
  }
  catch(const std::exception &)
  {
    // The swap function should not throw, for our test cases.
    BOOST_TEST(!"throw in swap");
  }
}

//
// Tests whether the optional<T>::swap member function works properly.
// Assumes that T has one data member, of type char.
// Returns true iff the test is passed.
//
template <class T>
void test_swap_member_function( T const* )
{
  try
  {
    optional<T> obj1;
    optional<T> obj2('a');

    // Self-swap should not have any effect.
    obj1.swap(obj1);
    obj2.swap(obj2);
    BOOST_TEST(!obj1);
    BOOST_TEST(!!obj2 && obj2->data == 'a');

    // Call member swap.
    obj1.swap(obj2);

    // Test if obj1 and obj2 are really swapped.
    BOOST_TEST(!!obj1 && obj1->data == 'a');
    BOOST_TEST(!obj2);

    // Call member swap one more time.
    obj1.swap(obj2);

    // Test if obj1 and obj2 are swapped back.
    BOOST_TEST(!obj1);
    BOOST_TEST(!!obj2 && obj2->data == 'a');
  }
  catch(const std::exception &)
  {
    BOOST_TEST(!"throw in swap");
  }
}


//
// Tests compile time tweaking of swap, by means of
// optional_swap_should_use_default_constructor.
//
void test_swap_tweaking()
{
  ( test_swap_function( ARG(optional_swap_test::class_without_default_ctor) ) );
  ( test_swap_function( ARG(optional_swap_test::class_whose_default_ctor_should_be_used) ) );
  ( test_swap_function( ARG(optional_swap_test::class_whose_default_ctor_should_not_be_used) ) );
  ( test_swap_function( ARG(optional_swap_test::class_whose_explicit_ctor_should_be_used) ) );
  ( test_swap_function( ARG(optional_swap_test::template_whose_default_ctor_should_be_used<char>) ) );
  ( test_swap_member_function( ARG(optional_swap_test::class_without_default_ctor) ) );
  ( test_swap_member_function( ARG(optional_swap_test::class_whose_default_ctor_should_be_used) ) );
  ( test_swap_member_function( ARG(optional_swap_test::class_whose_default_ctor_should_not_be_used) ) );
  ( test_swap_member_function( ARG(optional_swap_test::class_whose_explicit_ctor_should_be_used) ) );
  ( test_swap_member_function( ARG(optional_swap_test::template_whose_default_ctor_should_be_used<char>) ) );
}

int main()
{
  test_swap_tweaking();
  return boost::report_errors();
}
