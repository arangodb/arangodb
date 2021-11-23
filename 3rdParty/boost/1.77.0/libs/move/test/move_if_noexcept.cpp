//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Antony Polukhin 2014.
// (C) Copyright Ion Gaztanaga 2014.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/move/utility.hpp>
#include <boost/core/lightweight_test.hpp>
#include "../example/movable.hpp"
#include "../example/copymovable.hpp"
#include <boost/static_assert.hpp>

//////////////////////////////////////////////////////////////////////////////
//A copy_movable_noexcept class
class copy_movable_noexcept
{
   BOOST_COPYABLE_AND_MOVABLE(copy_movable_noexcept)
   int value_;

   public:
   copy_movable_noexcept() : value_(1){}

   //Move constructor and assignment
   copy_movable_noexcept(BOOST_RV_REF(copy_movable_noexcept) m)
   {  value_ = m.value_;   m.value_ = 0;  }

   copy_movable_noexcept(const copy_movable_noexcept &m)
   {  value_ = m.value_;   }

   copy_movable_noexcept & operator=(BOOST_RV_REF(copy_movable_noexcept) m)
   {  value_ = m.value_;   m.value_ = 0;  return *this;  }

   copy_movable_noexcept & operator=(BOOST_COPY_ASSIGN_REF(copy_movable_noexcept) m)
   {  value_ = m.value_;   return *this;  }

   bool moved() const //Observer
   {  return value_ == 0; }
};

namespace boost{

template<>
struct has_nothrow_move<copy_movable_noexcept>
{
   static const bool value = true;
};

}  //namespace boost{

//////////////////////////////////////////////////////////////////////////////
//A movable_throwable class
class movable_throwable
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(movable_throwable)
   int value_;

   public:
   movable_throwable() : value_(1){}

   //Move constructor and assignment
   movable_throwable(BOOST_RV_REF(movable_throwable) m)
   {  value_ = m.value_;   m.value_ = 0;  }

   movable_throwable & operator=(BOOST_RV_REF(movable_throwable) m)
   {  value_ = m.value_;   m.value_ = 0;  return *this;  }

   bool moved() const //Observer
   {  return !value_; }

   int value() const //Observer
   {  return value_; }
};


//////////////////////////////////////////////////////////////////////////////
// Helper functions
movable function(movable m)
{
   return movable(boost::move_if_noexcept(m));
}

copy_movable function(copy_movable m)
{
   return copy_movable(boost::move_if_noexcept(m));
}

copy_movable_noexcept function(copy_movable_noexcept m)
{
   return copy_movable_noexcept(boost::move_if_noexcept(m));
}

movable_throwable function(movable_throwable m)
{
   return movable_throwable(boost::move_if_noexcept(m));
}

movable functionr(BOOST_RV_REF(movable) m)
{
   return movable(boost::move_if_noexcept(m));
}

movable function2(movable m)
{
   return boost::move_if_noexcept(m);
}

BOOST_RV_REF(movable) function2r(BOOST_RV_REF(movable) m)
{
   return boost::move_if_noexcept(m);
}

movable move_return_function2 ()
{
    return movable();
}

movable move_return_function ()
{
    movable m;
    return (boost::move_if_noexcept(m));
}

#define BOOST_CHECK(x) if (!(x)) { return __LINE__; }

int main()
{
   {
      movable m;
      movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      movable m3(function(movable(boost::move_if_noexcept(m2))));
      BOOST_CHECK(m2.moved());
      movable m4(function(boost::move_if_noexcept(m3)));
      BOOST_CHECK(m3.moved());
      BOOST_CHECK(!m4.moved());
   }
   {
      movable m;
      movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      movable m3(functionr(movable(boost::move_if_noexcept(m2))));
      BOOST_CHECK(m2.moved());
      movable m4(functionr(boost::move_if_noexcept(m3)));
      BOOST_CHECK(m3.moved());
      BOOST_CHECK(!m4.moved());
   }
   {
      movable m;
      movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      movable m3(function2(movable(boost::move_if_noexcept(m2))));
      BOOST_CHECK(m2.moved());
      movable m4(function2(boost::move_if_noexcept(m3)));
      BOOST_CHECK(m3.moved());
      BOOST_CHECK(!m4.moved());
   }
   {
      movable m;
      movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      movable m3(function2r(movable(boost::move_if_noexcept(m2))));
      BOOST_CHECK(m2.moved());
      movable m4(function2r(boost::move_if_noexcept(m3)));
      BOOST_CHECK(m3.moved());
      BOOST_CHECK(!m4.moved());
   }
   {
      movable m;
      movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      BOOST_CHECK(!m2.moved());
      movable m3(move_return_function());
      BOOST_CHECK(!m3.moved());
   }
   {
      movable m;
      movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      BOOST_CHECK(!m2.moved());
      movable m3(move_return_function2());
      BOOST_CHECK(!m3.moved());
   }

   // copy_movable may throw during move, so it must be copied
   {
      copy_movable m;
      copy_movable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(!m.moved());
      copy_movable m3(function(copy_movable(boost::move_if_noexcept(m2))));
      BOOST_CHECK(!m2.moved());
      copy_movable m4(function(boost::move_if_noexcept(m3)));
      BOOST_CHECK(!m3.moved());
      BOOST_CHECK(!m4.moved());
   }


   // copy_movable_noexcept can not throw during move
   {
      copy_movable_noexcept m;
      copy_movable_noexcept m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      copy_movable_noexcept m3(function(copy_movable_noexcept(boost::move_if_noexcept(m2))));
      BOOST_CHECK(m2.moved());
      copy_movable_noexcept m4(function(boost::move_if_noexcept(m3)));
      BOOST_CHECK(m3.moved());
      BOOST_CHECK(!m4.moved());
   }

   // movable_throwable can not throw during move but it has no copy constructor
   {
      movable_throwable m;
      movable_throwable m2(boost::move_if_noexcept(m));
      BOOST_CHECK(m.moved());
      movable_throwable m3(function(movable_throwable(boost::move_if_noexcept(m2))));
      BOOST_CHECK(m2.moved());
      movable_throwable m4(function(boost::move_if_noexcept(m3)));
      BOOST_CHECK(m3.moved());
      BOOST_CHECK(!m4.moved());
   }

   return boost::report_errors();
}
