//  throw_test.cpp  --------------------------------------------------------===========-//

//  Copyright Beman Dawes 2010

//  Distributed under the Boost Software License, Version 1.0.
//  See www.boost.org/LICENSE_1_0.txt

//  Library home page is www.boost.org/libs/system

//--------------------------------------------------------------------------------------// 

//  See dynamic_link_test.cpp comments for use case.

//--------------------------------------------------------------------------------------// 

#include <boost/system/system_error.hpp>
#include <boost/config.hpp>

#if defined(THROW_DYN_LINK)
# define EXPORT BOOST_SYMBOL_EXPORT
#else
# define EXPORT
#endif

EXPORT void throw_test()
{
    throw boost::system::system_error( 9999, boost::system::system_category(), "boo boo" );
}
