//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/static_vector.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::static_vector<empty, 2>;
volatile ::boost::container::static_vector<empty, 2> dummy;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors
template class boost::container::static_vector<int, 10>;

}}

int main()
{
   return 0;
}
