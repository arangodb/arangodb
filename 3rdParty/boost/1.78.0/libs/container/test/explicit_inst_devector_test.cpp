//////////////////////////////////////////////////////////////////////////////
//
// \(C\) Copyright Benedek Thaler 2015-2016
// \(C\) Copyright Ion Gaztanaga 2019-2020. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/devector.hpp>
#include "movable_int.hpp"

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::devector<empty>;

namespace boost {
namespace container {

//Test stored_size option
template class devector< test::movable_and_copyable_int
                       , new_allocator<test::movable_and_copyable_int>
                       , devector_options< stored_size<unsigned short> >::type
                       >;


}  //namespace container {
}  //namespace boost {

int main()
{
   ::boost::container::devector<empty> dummy;
   (void)dummy;
   return 0;
}
