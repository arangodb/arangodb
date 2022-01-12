//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#define _WIN32_WINNT 0x0501
#define BOOST_USE_WINDOWS_H

#include <boost/interprocess/detail/workaround.hpp>

#ifdef BOOST_INTERPROCESS_WINDOWS

#include <boost/interprocess/windows_shared_memory.hpp>

using namespace boost::interprocess;

int main ()
{
   windows_shared_memory dummy;
   static_cast<void>(dummy);
   return 0;
}

#else

int main()
{
   return 0;
}

#endif
