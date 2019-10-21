//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_CMP_VIEW_HPP
#define BOOST_GIL_IO_TEST_CMP_VIEW_HPP

#include <boost/gil.hpp>

template< typename View_1, typename View_2 >
void cmp_view( const View_1& v1
             , const View_2& v2
             )
{
    if( v1.dimensions() != v2.dimensions() )
    {
        throw std::runtime_error( "Images are not equal." );
    }

    typename View_1::x_coord_t width  = v1.width();
    typename View_1::y_coord_t height = v1.height();

    for( typename View_1::y_coord_t y = 0; y < height; ++y )
    {
        const typename View_1::x_iterator src_it = v1.row_begin( y );
        const typename View_2::x_iterator dst_it = v2.row_begin( y );

        for( typename View_1::x_coord_t x = 0; x < width; ++x )
        {
            if( *src_it != *dst_it )
            {
                throw std::runtime_error( "Images are not equal." );
            }
        }
    }
}

#endif
