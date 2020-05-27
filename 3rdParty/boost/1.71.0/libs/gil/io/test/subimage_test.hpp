//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_TEST_SUBIMAGE_TEST_HPP
#define BOOST_GIL_IO_TEST_SUBIMAGE_TEST_HPP

#include <boost/gil.hpp>

using namespace std;
using namespace boost;
using namespace gil;

template< typename Image
        , typename Format
        >
void run_subimage_test( string filename
                      , const point_t& top_left
                      , const point_t& dimension
                      )
{
    Image original, subimage;

    read_image( filename
              , original
              , Format()
              );

    image_read_settings< Format > settings( top_left
                                           , dimension
                                           );


    read_image( filename
              , subimage
              , settings
              );

    BOOST_CHECK( equal_pixels( const_view( subimage )
                             , subimage_view( const_view( original )
                                            , top_left
                                            , dimension
                                            )
                             )
               );
}

#endif // BOOST_GIL_IO_TEST_SUBIMAGE_TEST_HPP
