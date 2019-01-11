//
// Copyright 2012 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_IO_PNM_DETAIL_WRITE_HPP
#define BOOST_GIL_EXTENSION_IO_PNM_DETAIL_WRITE_HPP

#include <boost/gil/extension/io/pnm/tags.hpp>
#include <boost/gil/extension/io/pnm/detail/writer_backend.hpp>

#include <boost/gil/io/base.hpp>
#include <boost/gil/io/device.hpp>

#include <cstdlib>
#include <string>

namespace boost { namespace gil {

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(push)
#pragma warning(disable:4512) //assignment operator could not be generated
#endif

namespace detail {

struct pnm_write_is_supported
{
    template< typename View >
    struct apply
        : public is_write_supported< typename get_pixel_type< View >::type
                                   , pnm_tag
                                   >
    {};
};

} // namespace detail

///
/// PNM Writer
///
template< typename Device >
class writer< Device
            , pnm_tag
            >
    : public writer_backend< Device
                           , pnm_tag
                           >
{
private:
    typedef writer_backend< Device, pnm_tag > backend_t;

public:

    writer( const Device&                      io_dev
          , const image_write_info< pnm_tag >& info
          )
    : backend_t( io_dev
                , info
                )
    {}

    template< typename View >
    void apply( const View& view )
    {
        typedef typename get_pixel_type< View >::type pixel_t;

        std::size_t width  = view.width();
        std::size_t height = view.height();

        std::size_t chn    = num_channels< View >::value;
        std::size_t pitch  = chn * width;

        unsigned int type = get_type< num_channels< View >::value >( is_bit_aligned< pixel_t >() );

        // write header

        // Add a white space at each string so read_int() can decide when a numbers ends.

        std::string str( "P" );
        str += std::to_string( type ) + std::string( " " );
        this->_io_dev.print_line( str );

        str.clear();
        str += std::to_string( width ) + std::string( " " );
        this->_io_dev.print_line( str );

        str.clear();
        str += std::to_string( height ) + std::string( " " );
        this->_io_dev.print_line( str );

        if( type != pnm_image_type::mono_bin_t::value )
        {
            this->_io_dev.print_line( "255 ");
        }

        // write data
        write_data( view
                  , pitch
                  , typename is_bit_aligned< pixel_t >::type()
                  );
    }

private:

    template< int Channels >
    unsigned int get_type( mpl::true_  /* is_bit_aligned */ )
    {
        return boost::mpl::if_c< Channels == 1
                               , pnm_image_type::mono_bin_t
                               , pnm_image_type::color_bin_t
                               >::type::value;
    }

    template< int Channels >
    unsigned int get_type( mpl::false_ /* is_bit_aligned */ )
    {
        return boost::mpl::if_c< Channels == 1
                               , pnm_image_type::gray_bin_t
                               , pnm_image_type::color_bin_t
                               >::type::value;
    }

    template< typename View >
    void write_data( const View&   src
                   , std::size_t   pitch
                   , const mpl::true_&    // bit_aligned
                   )
    {
        BOOST_STATIC_ASSERT(( is_same< View
                                     , typename gray1_image_t::view_t
                                     >::value
                           ));

        byte_vector_t row( pitch / 8 );

        typedef typename View::x_iterator x_it_t;
        x_it_t row_it = x_it_t( &( *row.begin() ));

        detail::negate_bits< byte_vector_t
                           , mpl::true_
                           > neg;

        detail::mirror_bits< byte_vector_t
                           , mpl::true_
                           > mirror;


        for( typename View::y_coord_t y = 0; y < src.height(); ++y )
        {
            std::copy( src.row_begin( y )
                     , src.row_end( y )
                     , row_it
                     );

            mirror( row );
            neg   ( row );

            this->_io_dev.write( &row.front()
                               , pitch / 8
                               );
        }
    }

    template< typename View >
    void write_data( const View&   src
                   , std::size_t   pitch
                   , const mpl::false_&    // bit_aligned
                   )
    {
        std::vector< pixel< typename channel_type< View >::type
                          , layout<typename color_space_type< View >::type >
                          >
                   > buf( src.width() );

        // typedef typename View::value_type pixel_t;
        // typedef typename view_type_from_pixel< pixel_t >::type view_t;

        //view_t row = interleaved_view( src.width()
        //                             , 1
        //                             , reinterpret_cast< pixel_t* >( &buf.front() )
        //                             , pitch
        //                             );

        byte_t* row_addr = reinterpret_cast< byte_t* >( &buf.front() );

        for( typename View::y_coord_t y = 0
           ; y < src.height()
           ; ++y
           )
        {
            //copy_pixels( subimage_view( src
            //                          , 0
            //                          , (int) y
            //                          , (int) src.width()
            //                          , 1
            //                          )
            //           , row
            //           );

            std::copy( src.row_begin( y )
                     , src.row_end  ( y )
                     , buf.begin()
                     );

            this->_io_dev.write( row_addr, pitch );
        }
    }
};

///
/// PNM Writer
///
template< typename Device >
class dynamic_image_writer< Device
                          , pnm_tag
                          >
    : public writer< Device
                   , pnm_tag
                   >
{
    typedef writer< Device
                  , pnm_tag
                  > parent_t;

public:

    dynamic_image_writer( const Device&                      io_dev
                        , const image_write_info< pnm_tag >& info
                        )
    : parent_t( io_dev
              , info
              )
    {}

    template< typename Views >
    void apply( const any_image_view< Views >& views )
    {
        detail::dynamic_io_fnobj< detail::pnm_write_is_supported
                                , parent_t
                                > op( this );

        apply_operation( views, op );
    }
};

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(pop)
#endif

} // gil
} // boost

#endif
