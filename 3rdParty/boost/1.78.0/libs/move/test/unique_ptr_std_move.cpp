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
#define BOOST_MOVE_USE_STANDARD_LIBRARY_MOVE
#include <boost/move/unique_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   //Just test compilation errors
   boost::movelib::unique_ptr<int> a, b(boost::move(a));
   BOOST_TEST(!(b.get() || a.get()));
   b = boost::move(a);
   b.release();
   BOOST_TEST(!(b.get() || a.get()));
   //Test results
   return boost::report_errors();
}
