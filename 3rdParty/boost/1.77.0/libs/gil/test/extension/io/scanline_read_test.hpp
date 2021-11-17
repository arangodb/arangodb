//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_SCANLINE_READ_TEST_HPP
#define BOOST_GIL_IO_TEST_SCANLINE_READ_TEST_HPP

#include <boost/gil.hpp>

#include "cmp_view.hpp"

template< typename Image
        , typename FormatTag
        >
void test_scanline_reader( const char* file_name )
{
    using namespace boost::gil;

    // read image using scanline_read_iterator
    using reader_t = scanline_reader
        <
            typename get_read_device<char const*, FormatTag>::type,
            FormatTag
        >;

    reader_t reader = make_scanline_reader( file_name, FormatTag() );

    Image dst( reader._info._width, reader._info._height );

    using iterator_t = typename reader_t::iterator_t;

    iterator_t it  = reader.begin();
    iterator_t end = reader.end();

    for( int row = 0; it != end; ++it, ++row )
    {
        copy_pixels( interleaved_view( reader._info._width
                                     , 1
                                     , ( typename Image::view_t::x_iterator ) *it
                                     , reader._scanline_length
                                     )
                   , subimage_view( view( dst )
                                  , 0
                                  , row
                                  , reader._info._width
                                  , 1
                                  )
                   );
    }

    //compare
    Image img;
    read_image( file_name, img, FormatTag() );

    cmp_view( view( dst ), view( img ) );
}

#endif // BOOST_GIL_IO_TEST_SCANLINE_READ_TEST_HPP
