//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Howard Hinnant 2009
// (C) Copyright Ion Gaztanaga 2014-2014.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_BEG_HPP
#define BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_BEG_HPP
#include <boost/move/core.hpp>
#include <boost/move/detail/unique_ptr_meta_utils.hpp>
#include <boost/static_assert.hpp>

//////////////////////////////////////////////
//
// The initial implementation of these tests
// was written by Howard Hinnant. 
//
// These test were later refactored grouping
// and porting them to Boost.Move.
//
// Many thanks to Howard for releasing his C++03
// unique_ptr implementation with such detailed
// test cases.
//
//////////////////////////////////////////////

//A deleter that can only default constructed
template <class T>
class def_constr_deleter
{
   int state_;
   def_constr_deleter(const def_constr_deleter&);
   def_constr_deleter& operator=(const def_constr_deleter&);

   public:
   typedef typename ::boost::move_upmu::remove_extent<T>::type element_type;
   static const bool is_array = ::boost::move_upmu::is_array<T>::value;

   def_constr_deleter() : state_(5) {}

   explicit def_constr_deleter(int s) : state_(s) {}

   int state() const {return state_;}

   void set_state(int s) {state_ = s;}

   void operator()(element_type* p) const
   {  is_array ? delete []p : delete p;   }

   void operator()(element_type* p)
   {  ++state_;   is_array ? delete []p : delete p;  }
};

//A deleter that can be copy constructed
template <class T>
class copy_constr_deleter
{
   int state_;

   public:
   typedef typename ::boost::move_upmu::remove_extent<T>::type element_type;
   static const bool is_array = ::boost::move_upmu::is_array<T>::value;

   copy_constr_deleter() : state_(5) {}

   template<class U>
   copy_constr_deleter(const copy_constr_deleter<U>&
      , typename boost::move_upd::enable_def_del<U, T>::type* =0)
   {  state_ = 5; }

   explicit copy_constr_deleter(int s) : state_(s) {}

   template <class U>
   typename boost::move_upd::enable_def_del<U, T, copy_constr_deleter&>::type
      operator=(const copy_constr_deleter<U> &d)
   {
      state_ = d.state();
      return *this;
   }

   int state() const          {return state_;}

   void set_state(int s)      {state_ = s;}

   void operator()(element_type* p) const
   {  is_array ? delete []p : delete p;   }

   void operator()(element_type* p)
   {  ++state_;   is_array ? delete []p : delete p;  }
};

//A deleter that can be only move constructed
template <class T>
class move_constr_deleter
{
   int state_;

   BOOST_MOVABLE_BUT_NOT_COPYABLE(move_constr_deleter)

   public:
   typedef typename ::boost::move_upmu::remove_extent<T>::type element_type;
   static const bool is_array = ::boost::move_upmu::is_array<T>::value;

   move_constr_deleter() : state_(5) {}

   move_constr_deleter(BOOST_RV_REF(move_constr_deleter) r)
      : state_(r.state_)
   {  r.state_ = 0;  }

   explicit move_constr_deleter(int s) : state_(s) {}

   template <class U>
   move_constr_deleter(BOOST_RV_REF(move_constr_deleter<U>) d
      , typename boost::move_upd::enable_def_del<U, T>::type* =0)
      : state_(d.state())
   { d.set_state(0);  }

   move_constr_deleter& operator=(BOOST_RV_REF(move_constr_deleter) r)
   {
      state_ = r.state_;
      r.state_ = 0;
      return *this;
   }

   template <class U>
   typename boost::move_upd::enable_def_del<U, T, move_constr_deleter&>::type
      operator=(BOOST_RV_REF(move_constr_deleter<U>) d)
   {
      state_ = d.state();
      d.set_state(0);
      return *this;
   }

   int state() const          {return state_;}

   void set_state(int s)      {state_ = s;}

   void operator()(element_type* p) const
   {  is_array ? delete []p : delete p;   }

   void operator()(element_type* p)
   {  ++state_;   is_array ? delete []p : delete p;  }

   friend bool operator==(const move_constr_deleter& x, const move_constr_deleter& y)
      {return x.state_ == y.state_;}
};

//A base class containing state with a static instance counter
struct A
{
   int state_;
   static int count;

   A()               : state_(999)      {++count;}
   explicit A(int i) : state_(i)        {++count;}
   A(const A& a)     : state_(a.state_) {++count;}
   A& operator=(const A& a) { state_ = a.state_; return *this; }
   void set(int i)   {state_ = i;}
   virtual ~A()      {--count;}
   friend bool operator==(const A& x, const A& y) { return x.state_ == y.state_; }
};

int A::count = 0;

//A class derived from A with a static instance counter
struct B
   : public A
{
   static int count;
   B() : A() {++count;}
   B(const B &b) : A(b) {++count;}
   virtual ~B() {--count;}
};

int B::count = 0;

void reset_counters();

BOOST_STATIC_ASSERT((::boost::move_upmu::is_convertible<B, A>::value));

//Incomplete Type function declarations
struct I;
void check(int i);
I* get();
I* get_array(int i);

#include <boost/move/unique_ptr.hpp>

template <class T, class D = ::boost::movelib::default_delete<T> >
struct J
{
   typedef boost::movelib::unique_ptr<T, D> unique_ptr_type;
   typedef typename unique_ptr_type::element_type element_type;
   boost::movelib::unique_ptr<T, D> a_;
   J() {}
   explicit J(element_type*a) : a_(a) {}
   ~J();

   element_type* get() const {return a_.get();}
   D& get_deleter() {return a_.get_deleter();}
};

#endif   //BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_BEG_HPP
