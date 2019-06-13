//
// Copyright 2007-2008 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_ROW_BUFFER_HELPER_HPP
#define BOOST_GIL_IO_ROW_BUFFER_HELPER_HPP

// TODO: Shall we move toolbox to core?
#include <boost/gil/extension/toolbox/metafunctions/is_bit_aligned.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_homogeneous.hpp>
#include <boost/gil/extension/toolbox/metafunctions/pixel_bit_size.hpp>

#include <boost/gil/io/typedefs.hpp>

#include <boost/mpl/and.hpp>
#include <boost/utility/enable_if.hpp>

#include <cstddef>
#include <vector>

namespace boost { namespace gil { namespace detail {

template< typename Pixel
        , typename DummyT = void
        >
struct row_buffer_helper
{
    typedef Pixel element_t;
    typedef std::vector< element_t > buffer_t;
    typedef typename buffer_t::iterator iterator_t;

    row_buffer_helper( std::size_t width
                     , bool
                     )
    : _row_buffer( width )
    {}

    element_t* data() { return &_row_buffer[0]; }

    iterator_t begin() { return _row_buffer.begin(); }
    iterator_t end()   { return _row_buffer.end();   }

    buffer_t& buffer() { return _row_buffer; }

private:

    buffer_t _row_buffer;
};

template<typename Pixel >
struct row_buffer_helper< Pixel
                        , typename enable_if< typename is_bit_aligned< Pixel >::type >::type
                        >
{
    typedef byte_t element_t;
    typedef std::vector< element_t > buffer_t;
    typedef Pixel pixel_type;
    typedef bit_aligned_pixel_iterator<pixel_type> iterator_t;

    row_buffer_helper( std::size_t width
                     , bool        in_bytes
                     )
    : _c( ( width
          * pixel_bit_size< pixel_type >::value
          )
          >> 3
        )

    , _r( width
          * pixel_bit_size< pixel_type >::value
        - ( _c << 3 )
       )
    {
        if( in_bytes )
        {
            _row_buffer.resize( width );
        }
        else
        {
            // add one byte if there are remaining bits
            _row_buffer.resize( _c + ( _r!=0 ));
        }
    }

    element_t* data() { return &_row_buffer[0]; }

    iterator_t begin() { return iterator_t( &_row_buffer.front(),0 ); }
    iterator_t end()   { return _r == 0 ? iterator_t( &_row_buffer.back() + 1,  0 )
                                        : iterator_t( &_row_buffer.back()    , (int) _r );
                       }

    buffer_t& buffer() { return _row_buffer; }

private:

    // For instance 25 pixels of rgb2 type would be:
    // overall 25 pixels * 3 channels * 2 bits/channel = 150 bits
    // c = 18 bytes
    // r = 6 bits

    std::size_t _c; // number of full bytes
    std::size_t _r; // number of remaining bits

    buffer_t _row_buffer;
};

template<typename Pixel>
struct row_buffer_helper
<
    Pixel,
    typename boost::enable_if
    <
        typename mpl::and_
        <
            typename is_bit_aligned<Pixel>::type,
            typename is_homogeneous<Pixel>::type
        >::type
    >
>
{
    typedef byte_t element_t;
    typedef std::vector< element_t > buffer_t;
    typedef Pixel pixel_type;
    typedef bit_aligned_pixel_iterator<pixel_type> iterator_t;

    row_buffer_helper( std::size_t width
                     , bool        in_bytes
                     )
    : _c( ( width
          * num_channels< pixel_type >::type::value
          * channel_type< pixel_type >::type::num_bits
          )
          >> 3
        )

    , _r( width
        * num_channels< pixel_type >::type::value
        * channel_type< pixel_type >::type::num_bits
        - ( _c << 3 )
       )
    {
        if( in_bytes )
        {
            _row_buffer.resize( width );
        }
        else
        {
            // add one byte if there are remaining bits
            _row_buffer.resize( _c + ( _r!=0 ));
        }
    }

    element_t* data() { return &_row_buffer[0]; }

    iterator_t begin() { return iterator_t( &_row_buffer.front(),0 ); }
    iterator_t end()   { return _r == 0 ? iterator_t( &_row_buffer.back() + 1,  0 )
                                        : iterator_t( &_row_buffer.back()    , (int) _r );
                       }

    buffer_t& buffer() { return _row_buffer; }

private:

    // For instance 25 pixels of rgb2 type would be:
    // overall 25 pixels * 3 channels * 2 bits/channel = 150 bits
    // c = 18 bytes
    // r = 6 bits

    std::size_t _c; // number of full bytes
    std::size_t _r; // number of remaining bits

    buffer_t _row_buffer;
};

template< typename View
        , typename D = void
        >
struct row_buffer_helper_view : row_buffer_helper< typename View::value_type >
{
    row_buffer_helper_view( std::size_t width
                          , bool        in_bytes
                          )
    :  row_buffer_helper< typename View::value_type >( width
                                                     , in_bytes
                                                     )
    {}
};


template< typename View >
struct row_buffer_helper_view< View
                             , typename enable_if< typename is_bit_aligned< typename View::value_type
                                                                          >::type
                                                 >::type
                             > : row_buffer_helper< typename View::reference >
{
    row_buffer_helper_view( std::size_t width
                          , bool        in_bytes
                          )
    : row_buffer_helper< typename View::reference >( width
                                                   , in_bytes
                                                   )
    {}
};

} // namespace detail
} // namespace gil
} // namespace boost

#endif
