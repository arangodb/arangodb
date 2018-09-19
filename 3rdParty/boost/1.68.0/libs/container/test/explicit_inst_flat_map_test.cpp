//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/flat_map.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::flat_map<empty, empty>;
template class ::boost::container::flat_multimap<empty, empty>;

int main()
{
   ::boost::container::flat_map<empty, empty> dummy;
   ::boost::container::flat_multimap<empty, empty> dummy2;
   (void)dummy; (void)dummy2;
   return 0;
}
