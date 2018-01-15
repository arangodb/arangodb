//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/detail/config_begin.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/core.hpp>
#include <boost/core/lightweight_test.hpp>

class swap_stats
{
   public:
   static void reset_stats()
   {
      member_swap_calls = 0;
      friend_swap_calls = 0;
      move_cnstor_calls = 0;
      move_assign_calls = 0;
      copy_cnstor_calls = 0;
      copy_assign_calls = 0;
   }

   static unsigned int member_swap_calls;
   static unsigned int friend_swap_calls;
   static unsigned int move_cnstor_calls;
   static unsigned int move_assign_calls;
   static unsigned int copy_cnstor_calls;
   static unsigned int copy_assign_calls;
};

unsigned int swap_stats::member_swap_calls = 0;
unsigned int swap_stats::friend_swap_calls = 0;
unsigned int swap_stats::move_cnstor_calls = 0;
unsigned int swap_stats::move_assign_calls = 0;
unsigned int swap_stats::copy_cnstor_calls = 0;
unsigned int swap_stats::copy_assign_calls = 0;

class movable : public swap_stats
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(movable)
   public:
   movable()                                 {}
   movable(BOOST_RV_REF(movable))            { ++move_cnstor_calls; }
   movable & operator=(BOOST_RV_REF(movable)){ ++move_assign_calls; return *this; }
   friend void swap(movable &, movable &)    { ++friend_swap_calls; }
};

class movable_swap_member : public swap_stats
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(movable_swap_member)
   public:
   movable_swap_member()                                             {}
   movable_swap_member(BOOST_RV_REF(movable_swap_member))            { ++move_cnstor_calls; }
   movable_swap_member & operator=(BOOST_RV_REF(movable_swap_member)){ ++move_assign_calls; return *this; }
   void swap(movable_swap_member &)                                  { ++member_swap_calls; }
   friend void swap(movable_swap_member &, movable_swap_member &)    { ++friend_swap_calls; }
};

class copyable : public swap_stats
{
   public:
   copyable()                                {}
   copyable(const copyable &)                { ++copy_cnstor_calls; }
   copyable & operator=(const copyable&)     { ++copy_assign_calls; return *this; }
   void swap(copyable &)                     { ++member_swap_calls; }
   friend void swap(copyable &, copyable &)  { ++friend_swap_calls; }
};

class no_swap : public swap_stats
{
   private: unsigned m_state;
   public:
   explicit no_swap(unsigned i): m_state(i){}
   no_swap(const no_swap &x)               { m_state = x.m_state; ++copy_cnstor_calls; }
   no_swap & operator=(const no_swap& x)   { m_state = x.m_state; ++copy_assign_calls; return *this; }
   void swap(no_swap &)                    { ++member_swap_calls; }
   friend bool operator==(const no_swap &x, const no_swap &y) {  return x.m_state == y.m_state; }
   friend bool operator!=(const no_swap &x, const no_swap &y) {  return !(x==y); }
};


int main()
{
   {  //movable
      movable x, y;
      swap_stats::reset_stats();
      ::boost::adl_move_swap(x, y);
      #if defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
      //In non rvalue reference compilers,
      //movable classes with no swap() member uses
      //boost::move() to implement swap.
      BOOST_TEST(swap_stats::friend_swap_calls == 0);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::move_cnstor_calls == 1);
      BOOST_TEST(swap_stats::move_assign_calls == 2);
      BOOST_TEST(swap_stats::copy_cnstor_calls == 0);
      BOOST_TEST(swap_stats::copy_assign_calls == 0);
      #else
      //In compilers with rvalue references, this should call friend swap via ADL
      BOOST_TEST(swap_stats::friend_swap_calls == 1);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::move_cnstor_calls == 0);
      BOOST_TEST(swap_stats::move_assign_calls == 0);
      BOOST_TEST(swap_stats::copy_cnstor_calls == 0);
      BOOST_TEST(swap_stats::copy_assign_calls == 0);
      #endif
   }
   {  //movable_swap_member
      movable_swap_member x, y;
      swap_stats::reset_stats();
      ::boost::adl_move_swap(x, y);
      #if defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
      //In non rvalue reference compilers,
      //movable classes with no swap() member uses
      //boost::move() to implement swap.
      BOOST_TEST(swap_stats::friend_swap_calls == 0);
      BOOST_TEST(swap_stats::member_swap_calls == 1);
      BOOST_TEST(swap_stats::move_cnstor_calls == 0);
      BOOST_TEST(swap_stats::move_assign_calls == 0);
      BOOST_TEST(swap_stats::copy_cnstor_calls == 0);
      BOOST_TEST(swap_stats::copy_assign_calls == 0);
      #else
      //In compilers with rvalue references, this should call friend swap via ADL
      BOOST_TEST(swap_stats::friend_swap_calls == 1);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::move_cnstor_calls == 0);
      BOOST_TEST(swap_stats::move_assign_calls == 0);
      BOOST_TEST(swap_stats::copy_cnstor_calls == 0);
      BOOST_TEST(swap_stats::copy_assign_calls == 0);
      #endif
   }
   {  //copyable
      copyable x, y;
      swap_stats::reset_stats();
      ::boost::adl_move_swap(x, y);
      //This should call friend swap via ADL
      BOOST_TEST(swap_stats::friend_swap_calls == 1);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::move_cnstor_calls == 0);
      BOOST_TEST(swap_stats::move_assign_calls == 0);
      BOOST_TEST(swap_stats::copy_cnstor_calls == 0);
      BOOST_TEST(swap_stats::copy_assign_calls == 0);
   }
   {  //no_swap
      no_swap x(1), y(2), x_back(x), y_back(y);
      swap_stats::reset_stats();
      ::boost::adl_move_swap(x, y);
      //This should call std::swap which uses copies
      BOOST_TEST(swap_stats::friend_swap_calls == 0);
      BOOST_TEST(swap_stats::member_swap_calls == 0);
      BOOST_TEST(swap_stats::move_cnstor_calls == 0);
      BOOST_TEST(swap_stats::move_assign_calls == 0);
      BOOST_TEST(swap_stats::copy_cnstor_calls == 1);
      BOOST_TEST(swap_stats::copy_assign_calls == 2);
      BOOST_TEST(x == y_back);
      BOOST_TEST(y == x_back);
      BOOST_TEST(x != y);
   }
   return ::boost::report_errors();
}
#include <boost/move/detail/config_end.hpp>
