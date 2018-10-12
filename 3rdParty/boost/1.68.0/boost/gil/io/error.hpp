/*
    Copyright 2007-2008 Christian Henning, Andreas Pokorny, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_IO_ERROR_HPP
#define BOOST_GIL_IO_ERROR_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date   2010 \n
///
////////////////////////////////////////////////////////////////////////////////////////

namespace boost { namespace gil {

inline 
void io_error( const char* descr )
{
   throw std::ios_base::failure( descr );
}

inline 
void io_error_if( bool expr, const char* descr )
{
   if( expr ) 
      io_error( descr );
}

} // namespace gil
} // namespace boost

#endif
