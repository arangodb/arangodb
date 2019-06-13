//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright David Abrahams, Vicente Botet, Ion Gaztanaga 2009.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/detail/config_begin.hpp>
#include <boost/move/iterator.hpp>
#include <boost/container/vector.hpp>
#include <boost/core/lightweight_test.hpp>
#include "../example/movable.hpp"

int main()
{
   namespace bc = ::boost::container;
   //Default construct 10 movable objects
   bc::vector<movable> v(10);

   //Test default constructed value
   BOOST_TEST(!v[0].moved());

   //Move values
   bc::vector<movable> v2
      (boost::make_move_iterator(v.begin()), boost::make_move_iterator(v.end()));

   //Test values have been moved
   BOOST_TEST(v[0].moved());
   BOOST_TEST(v2.size() == 10);

   //Move again
   v.assign(boost::make_move_iterator(v2.begin()), boost::make_move_iterator(v2.end()));

   //Test values have been moved
   BOOST_TEST(v2[0].moved());
   BOOST_TEST(!v[0].moved());

   return ::boost::report_errors();
}

#include <boost/move/detail/config_end.hpp>
