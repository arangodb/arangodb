///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/throw_exception.hpp>
#include <boost/config.hpp>

#ifdef BOOST_NO_EXCEPTIONS

#include <iostream>
#include<iomanip>

namespace boost {

   void throw_exception(std::exception const & e)
   {
      std::cerr << "Terminating with exception: " << e.what() << std::endl;
   }

}

#else

namespace boost { namespace detail { void dummy_proc() {} } }

#endif

