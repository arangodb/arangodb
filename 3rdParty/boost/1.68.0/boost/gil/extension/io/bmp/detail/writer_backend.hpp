/*
    Copyright 2012 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_BMP_DETAIL_WRITER_BACKEND_HPP
#define BOOST_GIL_EXTENSION_IO_BMP_DETAIL_WRITER_BACKEND_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2012 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/extension/io/bmp/tags.hpp>

namespace boost { namespace gil {

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) 
#pragma warning(push) 
#pragma warning(disable:4512) //assignment operator could not be generated 
#endif

///
/// BMP Writer Backend
///
template< typename Device >
struct writer_backend< Device
                     , bmp_tag
                     >
{
public:

    typedef bmp_tag format_tag_t;

public:

    writer_backend( const Device&                      io_dev
                  , const image_write_info< bmp_tag >& info
                  )
    : _io_dev( io_dev )
    , _info  ( info   )
    {}

public:

    Device _io_dev;

    image_write_info< bmp_tag > _info;
};

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) 
#pragma warning(pop) 
#endif 

} // namespace gil
} // namespace boost

#endif
