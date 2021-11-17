/* Copyright 2004 Jonathan Brandmeyer 
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * The purpose of this test is to determine if a function can be called from
 * Python with a const value type as an argument, and whether or not the 
 * presence of a prototype without the cv-qualifier will work around the
 * compiler's bug.
 */
#include <boost/python.hpp>
using namespace boost::python;


#if BOOST_WORKAROUND(BOOST_MSVC, == 1200)
bool accept_const_arg( object );
#endif

bool accept_const_arg( const object )
{
    return true; 
}


BOOST_PYTHON_MODULE( const_argument_ext )
{
    def( "accept_const_arg", accept_const_arg );
}
