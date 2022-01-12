//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifdef _MSC_VER
//#pragma warning(disable : 4244)     // conversion from 'gil::image<V,Alloc>::coord_t' to 'int', possible loss of data (visual studio compiler doesn't realize that the two types are the same)
#pragma warning(disable : 4503)     // decorated name length exceeded, name was truncated
#endif

#include <boost/gil/extension/dynamic_image/dynamic_image_all.hpp>

#include <boost/core/lightweight_test.hpp>

#include <ios>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

using namespace boost::gil;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////

std::size_t is_planar_impl( const std::size_t size_in_units
                            , const std::size_t channels_in_image
                            , std::true_type
                            )
{
    return size_in_units * channels_in_image;
}

std::size_t is_planar_impl( const std::size_t size_in_units
                          , const std::size_t
                          , std::false_type
                          )
{
    return size_in_units;
}

template< typename View >
std::size_t get_row_size_in_memunits( typename View::x_coord_t width)
{   // number of units per row
    std::size_t size_in_memunits = width * memunit_step( typename View::x_iterator() );

    return size_in_memunits;
}


template< typename View
        , bool IsPlanar
        >
std::size_t total_allocated_size_in_bytes( const typename View::point_t& dimensions )
{

    using x_iterator = typename View::x_iterator;

    // when value_type is a non-pixel, like int or float, num_channels< ... > doesn't work.
    const std::size_t _channels_in_image = mp11::mp_eval_if< is_pixel< typename View::value_type >
                                                        , num_channels< View >
                                                        , std::integral_constant<int, 1>
                                                        >::type::value;

    std::size_t size_in_units = is_planar_impl( get_row_size_in_memunits< View >( dimensions.x ) * dimensions.y
                                                , _channels_in_image
                                                , typename std::conditional< IsPlanar, std::true_type, std::false_type >::type()
                                                );

    // return the size rounded up to the nearest byte
    std::size_t btm = byte_to_memunit< typename View::x_iterator >::value;


    return ( size_in_units + btm - 1 )
           / btm;
}


void test_recreate_image()
{
    auto tasib_1 = total_allocated_size_in_bytes<rgb8_view_t, false>({640, 480});
    auto tasib_2 = total_allocated_size_in_bytes<rgb8_view_t, false>({320, 200});

    rgb8_image_t img( 640, 480 );
    img.recreate( 320, 200 );
}

int main()
{
    test_recreate_image();

    return ::boost::report_errors();
}


#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(push)
#pragma warning(disable:4127) //conditional expression is constant
#endif

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(pop)
#endif
