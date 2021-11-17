//////////////////////////////////////////////////////////////////////////////
//
// \(C\) Copyright Benedek Thaler 2015-2016
// \(C\) Copyright Ion Gaztanaga 2019-2020. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://erenon.hu/double_ended for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_TEST_ELEM_HPP
#define BOOST_CONTAINER_TEST_TEST_ELEM_HPP

#include <boost/utility/compare_pointees.hpp>
#include <cstdlib>

namespace boost {
namespace container {

struct test_exception {};

struct test_elem_throw
{
   private:
   static int throw_on_ctor_after /*= -1*/;
   static int throw_on_copy_after /*= -1*/;
   static int throw_on_move_after /*= -1*/;

   public:
   static void on_ctor_after(int x) { throw_on_ctor_after = x; }
   static void on_copy_after(int x) { throw_on_copy_after = x; }
   static void on_move_after(int x) { throw_on_move_after = x; }

   static void do_not_throw()
   {
      throw_on_ctor_after = -1;
      throw_on_copy_after = -1;
      throw_on_move_after = -1;
   }

   static void in_constructor() { maybe_throw(throw_on_ctor_after); }
   static void in_copy() { maybe_throw(throw_on_copy_after); }
   static void in_move() { maybe_throw(throw_on_move_after); }

   private:
   static void maybe_throw(int& counter)
   {
      if (counter > 0)
      {
      --counter;
      if (counter == 0)
      {
         --counter;
         #ifndef BOOST_NO_EXCEPTIONS
         throw test_exception();
         #else
         std::abort();
         #endif
      }
      }
   }
};

int test_elem_throw::throw_on_ctor_after = -1;
int test_elem_throw::throw_on_copy_after = -1;
int test_elem_throw::throw_on_move_after = -1;

struct test_elem_base
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(test_elem_base)

   public:
   test_elem_base()
   {
      test_elem_throw::in_constructor();
      _index = new int(0);
      ++_live_count;
   }

   test_elem_base(int index)
   {
      test_elem_throw::in_constructor();
      _index = new int(index);
      ++_live_count;
   }

   explicit test_elem_base(const test_elem_base& rhs)
   {
      test_elem_throw::in_copy();
      _index = new int(*rhs._index);
      ++_live_count;
   }

   test_elem_base(BOOST_RV_REF(test_elem_base) rhs)
   {
      test_elem_throw::in_move();
      _index = rhs._index;
      rhs._index = 0;
      ++_live_count;
   }

   test_elem_base &operator=(BOOST_COPY_ASSIGN_REF(test_elem_base) rhs)
   {
      test_elem_throw::in_copy();
      if (_index) { delete _index; }
      _index = new int(*rhs._index);
      return *this;
   }

   test_elem_base &operator=(BOOST_RV_REF(test_elem_base) rhs)
   {
      test_elem_throw::in_move();
      if (_index) { delete _index; }
      _index = rhs._index;
      rhs._index = 0;
      return *this;
   }

   ~test_elem_base()
   {
      if (_index) { delete _index; }
      --_live_count;
   }

   friend bool operator==(const test_elem_base& a, const test_elem_base& b)
   {
      return a._index && b._index && *(a._index) == *(b._index);
   }

   friend bool operator==(int a, const test_elem_base& b)
   {
      return b._index != 0 && a == *(b._index);
   }

   friend bool operator==(const test_elem_base& a, int b)
   {
      return a._index != 0 && *(a._index) == b;
   }

   friend bool operator<(const test_elem_base& a, const test_elem_base& b)
   {
      return boost::less_pointees(a._index, b._index);
   }

   friend std::ostream& operator<<(std::ostream& out, const test_elem_base& elem)
   {
      if (elem._index) { out << *elem._index; }
      else { out << "null"; }
      return out;
   }

   template <typename Archive>
   void serialize(Archive& ar, unsigned /* version */)
   {
      ar & *_index;
   }

   static bool no_living_elem()
   {
      return _live_count == 0;
   }

   private:
   int* _index;

   static int _live_count;
};

int test_elem_base::_live_count = 0;

struct regular_elem : test_elem_base
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(regular_elem)

   public:
   regular_elem()
   {}

   regular_elem(int index) : test_elem_base(index) {}

   regular_elem(const regular_elem& rhs)
      :test_elem_base(rhs)
   {}

   regular_elem(BOOST_RV_REF(regular_elem) rhs)
      :test_elem_base(BOOST_MOVE_BASE(test_elem_base, rhs))
   {}

   regular_elem &operator=(BOOST_COPY_ASSIGN_REF(regular_elem) rhs)
   {
      static_cast<test_elem_base&>(*this) = rhs;
      return *this;
   }

   regular_elem &operator=(BOOST_RV_REF(regular_elem) rhs)
   {
      regular_elem &r = rhs;
      static_cast<test_elem_base&>(*this) = boost::move(r);
      return *this;
   }
};

struct noex_move : test_elem_base
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(noex_move)

   public:
   noex_move()
   {}

   noex_move(int index) : test_elem_base(index) {}

   noex_move(const noex_move& rhs)
      :test_elem_base(rhs)
   {}

   noex_move(BOOST_RV_REF(noex_move) rhs) BOOST_NOEXCEPT
      :test_elem_base(BOOST_MOVE_BASE(test_elem_base, rhs))
   {}

   noex_move &operator=(BOOST_COPY_ASSIGN_REF(noex_move) rhs)
   {
      static_cast<test_elem_base&>(*this) = rhs;
      return *this;
   }

   noex_move &operator=(BOOST_RV_REF(noex_move) rhs) BOOST_NOEXCEPT
   {
      noex_move & r = rhs;
      static_cast<test_elem_base&>(*this) = boost::move(r);
      return *this;
   }
};

struct noex_copy : test_elem_base
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(noex_copy)

   public:
   noex_copy(){}

   noex_copy(int index) : test_elem_base(index) {}

   noex_copy(const noex_copy& rhs) BOOST_NOEXCEPT
      :test_elem_base(rhs)
   {}

   noex_copy(BOOST_RV_REF(noex_copy) rhs)
      :test_elem_base(BOOST_MOVE_BASE(test_elem_base, rhs))
   {}

   noex_copy &operator=(BOOST_COPY_ASSIGN_REF(noex_copy) rhs) BOOST_NOEXCEPT
   {
      static_cast<test_elem_base&>(*this) = rhs;
      return *this;
   }

   noex_copy &operator=(BOOST_RV_REF(noex_copy) rhs)
   {
      noex_copy &r = rhs;
      static_cast<test_elem_base&>(*this) = boost::move(r);
      return *this;
   }
};

struct only_movable : test_elem_base
{
   private:
   BOOST_MOVABLE_BUT_NOT_COPYABLE(only_movable)

   public:
   only_movable(){};

   only_movable(int index) : test_elem_base(index) {}

   only_movable(BOOST_RV_REF(only_movable) rhs)
      :test_elem_base(BOOST_MOVE_BASE(test_elem_base, rhs))
   {}

   only_movable &operator=(BOOST_RV_REF(only_movable) rhs)
   {
      static_cast<test_elem_base&>(*this) = boost::move(rhs);
      return *this;
   }
};

struct no_default_ctor : test_elem_base
{

   private:
   BOOST_COPYABLE_AND_MOVABLE(no_default_ctor)

   public:
   no_default_ctor(int index) : test_elem_base(index) {}

   no_default_ctor(const no_default_ctor& rhs)
      :test_elem_base(rhs)
   {}

   no_default_ctor(BOOST_RV_REF(no_default_ctor) rhs)
      :test_elem_base(BOOST_MOVE_BASE(test_elem_base, rhs))
   {}

  no_default_ctor &operator=(BOOST_RV_REF(no_default_ctor) rhs)
  {
      static_cast<test_elem_base&>(*this) = boost::move(rhs);
      return *this;
  }

  no_default_ctor &operator=(BOOST_COPY_ASSIGN_REF(no_default_ctor) rhs)
  {
      static_cast<test_elem_base&>(*this) = rhs;
      return *this;
  }
};

}}

#endif   //BOOST_CONTAINER_TEST_TEST_ELEM_HPP
