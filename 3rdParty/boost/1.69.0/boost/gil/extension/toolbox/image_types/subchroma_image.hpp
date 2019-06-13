//
// Copyright 2013 Christian Henning and Juan V. Puertos
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_TOOLBOX_IMAGE_TYPES_SUBCHROMA_IMAGE_HPP
#define BOOST_GIL_EXTENSION_TOOLBOX_IMAGE_TYPES_SUBCHROMA_IMAGE_HPP

#include <boost/gil/image.hpp>
#include <boost/gil/point.hpp>

#include <boost/mpl/divides.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/or.hpp>
#include <boost/mpl/vector_c.hpp>

#include <cstddef>
#include <memory>

namespace boost{ namespace gil {

////////////////////////////////////////////////////////////////////////////////////////
/// \class subchroma_image_deref_fn
/// \ingroup PixelLocatorModel PixelBasedModel
/// \brief Used for virtual_2D_locator
///
////////////////////////////////////////////////////////////////////////////////////////
template< typename Locator
        , typename Factors
        >
struct subchroma_image_deref_fn
{
    typedef gray8_view_t::locator plane_locator_t;

    typedef subchroma_image_deref_fn     const_t;
    typedef typename Locator::value_type value_type;
    typedef value_type                   reference;
    typedef value_type                   const_reference;
    typedef point_t                      argument_type;
    typedef reference                    result_type;

    static const bool is_mutable = false;

    /// default constructor
    subchroma_image_deref_fn() {}

    /// constructor
    subchroma_image_deref_fn( const plane_locator_t& y_locator
                            , const plane_locator_t& v_locator
                            , const plane_locator_t& u_locator
                             )
    : _y_locator( y_locator )
    , _v_locator( v_locator )
    , _u_locator( u_locator )
    {}

    /// operator()
    result_type operator()( const point_t& p ) const
    {
        typedef Scaling_Factors< mpl::at_c< Factors, 0 >::type::value
                               , mpl::at_c< Factors, 1 >::type::value
                               , mpl::at_c< Factors, 2 >::type::value
                               > scaling_factors_t;

        plane_locator_t y = _y_locator.xy_at( p );
        plane_locator_t v = _v_locator.xy_at( p.x / scaling_factors_t::ss_X, p.y / scaling_factors_t::ss_X );
        plane_locator_t u = _u_locator.xy_at( p.x / scaling_factors_t::ss_X, p.y / scaling_factors_t::ss_X );

        return value_type( at_c< 0 >( *y )
                         , at_c< 0 >( *v )
                         , at_c< 0 >( *u )
                         );
    }

    ///
    const plane_locator_t& y_locator() const { return _y_locator; }
    const plane_locator_t& v_locator() const { return _v_locator; }
    const plane_locator_t& u_locator() const { return _u_locator; }

private:

    plane_locator_t _y_locator;
    plane_locator_t _v_locator;
    plane_locator_t _u_locator;
};


////////////////////////////////////////////////////////////////////////////////////////
/// \class subchroma_image_locator_type
/// \ingroup PixelLocatorModel PixelBasedModel
/// \brief
///
////////////////////////////////////////////////////////////////////////////////////////
template< typename Locator
        , typename Factors
        >
struct subchroma_image_locator
{
    typedef virtual_2d_locator< subchroma_image_deref_fn< Locator
                                                        , Factors
                                                        > // Deref
                              , false // IsTransposed
                              > type;
};


/////////////////////////////
//  PixelBasedConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct channel_type< subchroma_image_locator< Locator, Factors > >
    : public channel_type< typename subchroma_image_locator< Locator, Factors >::type > {};

template < typename Locator, typename Factors >
struct color_space_type< subchroma_image_locator< Locator, Factors > >
    : public color_space_type< typename subchroma_image_locator< Locator, Factors >::type > {};

template < typename Locator, typename Factors >
struct channel_mapping_type< subchroma_image_locator< Locator, Factors > >
    : public channel_mapping_type< typename subchroma_image_locator< Locator, Factors >::type > {};

template < typename Locator, typename Factors >
struct is_planar< subchroma_image_locator< Locator, Factors > >
    : public is_planar< typename subchroma_image_locator< Locator, Factors >::type > {};

/////////////////////////////
//  HasDynamicXStepTypeConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct dynamic_x_step_type< subchroma_image_locator< Locator, Factors > >
{
    typedef typename subchroma_image_locator< Locator, Factors >::type type;
};

/////////////////////////////
//  HasDynamicYStepTypeConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct dynamic_y_step_type< subchroma_image_locator< Locator, Factors > >
{
    typedef typename subchroma_image_locator< Locator, Factors >::type type;
};

/////////////////////////////
//  HasTransposedTypeConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct transposed_type< subchroma_image_locator< Locator, Factors > >
{
    typedef typename subchroma_image_locator< Locator, Factors >::type type;
};

//////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
/// \class subchroma_image_view
/// \ingroup ImageViewModel PixelBasedModel
/// \brief A lightweight object that interprets a subchroma image.
///
////////////////////////////////////////////////////////////////////////////////////////
template< typename Locator
        , typename Factors = mpl::vector_c< int, 4, 4, 4 >
        >
class subchroma_image_view : public image_view< Locator >
{
public:

    typedef typename Locator                     locator;
    typedef typename locator::deref_fn_t         deref_fn_t;
    typedef typename deref_fn_t::plane_locator_t plane_locator_t;


    typedef subchroma_image_view const_t;

    typedef image_view< plane_locator_t > plane_view_t;

    /// default constructor
    subchroma_image_view()
    : image_view< Locator >()
    {}

    /// constructor
    subchroma_image_view( const point_t& y_dimensions
                        , const point_t& v_dimensions
                        , const point_t& u_dimensions
                        , const Locator& locator
                        )
    : image_view< Locator >( y_dimensions, locator )
    , _y_dimensions( y_dimensions )
    , _v_dimensions( v_dimensions )
    , _u_dimensions( u_dimensions )
    {}

    /// copy constructor
    template< typename Subchroma_View >
    subchroma_image_view( const Subchroma_View& v )
    : image_view< locator >( v )
    {}

    const point_t& v_ssfactors() const { return point_t( get_deref_fn().vx_ssfactor(), get_deref_fn().vx_ssfactor() ); }
    const point_t& u_ssfactors() const { return point_t( get_deref_fn().ux_ssfactor(), get_deref_fn().ux_ssfactor() ); }

    const point_t& y_dimension() const { return _y_dimensions; }
    const point_t& v_dimension() const { return _v_dimensions; }
    const point_t& u_dimension() const { return _u_dimensions; }

    const plane_locator_t& y_plane() const { return get_deref_fn().y_locator(); }
    const plane_locator_t& v_plane() const { return get_deref_fn().v_locator(); }
    const plane_locator_t& u_plane() const { return get_deref_fn().u_locator(); }

    const plane_view_t y_plane_view() const { return plane_view_t( _y_dimensions, y_plane() ); }
    const plane_view_t v_plane_view() const { return plane_view_t( _v_dimensions, v_plane() ); }
    const plane_view_t u_plane_view() const { return plane_view_t( _u_dimensions, u_plane() ); }


private:

    const deref_fn_t& get_deref_fn() const { return this->pixels().deref_fn(); }

private:

    template< typename Locator, typename Factors > friend class subchroma_image_view;

    point_t _y_dimensions;
    point_t _v_dimensions;
    point_t _u_dimensions;
};


/////////////////////////////
//  PixelBasedConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct channel_type< subchroma_image_view< Locator, Factors > >
    : public channel_type< Locator > {};

template < typename Locator, typename Factors >
struct color_space_type< subchroma_image_view< Locator, Factors > >
    : public color_space_type< Locator > {};

template < typename Locator, typename Factors >
struct channel_mapping_type< subchroma_image_view< Locator, Factors > >
     : public channel_mapping_type< Locator > {};

template < typename Locator, typename Factors >
struct is_planar< subchroma_image_view< Locator, Factors > >
    : public is_planar< Locator > {};

/////////////////////////////
//  HasDynamicXStepTypeConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct dynamic_x_step_type< subchroma_image_view< Locator, Factors > >
{
    typedef image_view< typename dynamic_x_step_type< Locator >::type > type;
};

/////////////////////////////
//  HasDynamicYStepTypeConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct dynamic_y_step_type< subchroma_image_view< Locator, Factors > >
{
    typedef image_view< typename dynamic_y_step_type< Locator >::type > type;
};

/////////////////////////////
//  HasTransposedTypeConcept
/////////////////////////////

template < typename Locator, typename Factors >
struct transposed_type< subchroma_image_view< Locator, Factors > >
{
    typedef image_view< typename transposed_type< Locator >::type > type;
};


/////////////////////////////////////////////////////////////
template< int J
        , int a
        , int b
        >
struct Scaling_Factors
{
    BOOST_STATIC_ASSERT(( mpl::equal_to< mpl::int_< J >, mpl::int_< 4 > >::type::value ));

    BOOST_STATIC_ASSERT(( mpl::or_< mpl::equal_to< mpl::int_< a >, mpl::int_< 4 > >
                                  , mpl::or_< mpl::equal_to< mpl::int_< a >, mpl::int_< 2 > >
                                            , mpl::equal_to< mpl::int_< a >, mpl::int_< 1 > >
                                            >
                                  >::type::value
                       ));

    BOOST_STATIC_ASSERT(( mpl::or_< mpl::equal_to< mpl::int_< b >, mpl::int_< 4 > >
                                  , mpl::or_< mpl::equal_to< mpl::int_< b >, mpl::int_< 2 > >
                                            , mpl::or_< mpl::equal_to< mpl::int_< b >, mpl::int_< 1 > >
                                                      , mpl::equal_to< mpl::int_< b >, mpl::int_< 0 > >
                                                      >
                                            >
                                  >::type::value
                       ));

    BOOST_STATIC_CONSTANT( int, ss_X = ( mpl::divides< mpl::int_< J >
                                                     , mpl::int_< a >
                                                     >::type::value )
                         );

    BOOST_STATIC_CONSTANT( int, ss_Y = ( mpl::if_< mpl::equal_to< mpl::int_< b >, mpl::int_< 0 > >
                                                 , mpl::int_< 2 >
                                                 , mpl::if_< mpl::equal_to< mpl::int_< a >, mpl::int_< b > >
                                                           , mpl::int_< 1 >
                                                           , mpl::int_< 4 >
                                                           >::type
                                                 >::type::value )
                         );
};



////////////////////////////////////////////////////////////////////////////////////////
/// \ingroup ImageModel PixelBasedModel
/// \brief container interface over image view. Models ImageConcept, PixelBasedConcept
///
/// A subchroma image holds a bunch of planes which don't need to have the same resolution.
///
////////////////////////////////////////////////////////////////////////////////////////
template< typename Pixel
        , typename Factors   = mpl::vector_c< int, 4, 4, 4 >
        , typename Allocator = std::allocator< unsigned char >
        >
class subchroma_image : public Scaling_Factors< mpl::at_c< Factors, 0 >::type::value
                                              , mpl::at_c< Factors, 1 >::type::value
                                              , mpl::at_c< Factors, 2 >::type::value
                                              >
{

public:

    typedef typename channel_type< Pixel >::type channel_t;
    typedef pixel< channel_t, gray_layout_t> pixel_t;

    typedef image< pixel_t, false, Allocator > plane_image_t;

    typedef typename plane_image_t::view_t plane_view_t;
    typedef typename plane_image_t::const_view_t plane_const_view_t;
    typedef typename plane_view_t::locator plane_locator_t;

    typedef typename view_type_from_pixel< Pixel >::type pixel_view_t;
    typedef typename pixel_view_t::locator pixel_locator_t;

    typedef typename subchroma_image_locator< pixel_locator_t
                                            , Factors
                                            >::type locator_t;

    typedef typename plane_image_t::coord_t x_coord_t;
    typedef typename plane_image_t::coord_t y_coord_t;

    typedef subchroma_image_view< locator_t, Factors > view_t;
    typedef typename view_t::const_t                   const_view_t;


    /// constructor
    subchroma_image( const x_coord_t y_width
                   , const y_coord_t y_height
                   )
    : _y_plane(        y_width,        y_height, 0, Allocator() )
    , _v_plane( y_width / ss_X, y_height / ss_Y, 0, Allocator() )
    , _u_plane( y_width / ss_X, y_height / ss_Y, 0, Allocator() )
    {
        init();
    }

public:

    view_t _view;

private:

    void init()
    {
        typedef subchroma_image_deref_fn< pixel_locator_t
                                        , Factors
                                        > defer_fn_t;

        defer_fn_t deref_fn( view( _y_plane ).xy_at( 0, 0 )
                           , view( _v_plane ).xy_at( 0, 0 )
                           , view( _u_plane ).xy_at( 0, 0 )
                           );

        // init a virtual_2d_locator
        locator_t locator( point_t( 0, 0 ) // p
                         , point_t( 1, 1 ) // step
                         , deref_fn
                         );

        _view = view_t( _y_plane.dimensions()
                      , _v_plane.dimensions()
                      , _u_plane.dimensions()
                      , locator
                      );
    }


private:

    plane_image_t _y_plane;
    plane_image_t _v_plane;
    plane_image_t _u_plane;
};


/////////////////////////////
//  PixelBasedConcept
/////////////////////////////

template < typename Pixel, typename Factors, typename Alloc >
struct channel_type< subchroma_image< Pixel, Factors, Alloc > >
    : public channel_type< Pixel > {};

template < typename Pixel, typename Factors, typename Alloc >
struct color_space_type< subchroma_image< Pixel, Factors, Alloc > >
    : public color_space_type< Pixel > {};

template < typename Pixel, typename Factors, typename Alloc >
struct channel_mapping_type<  subchroma_image< Pixel, Factors, Alloc > >
    : public channel_mapping_type< Pixel > {};

template < typename Pixel, typename Factors, typename Alloc >
struct is_planar< subchroma_image< Pixel, Factors, Alloc > >
    : public mpl::bool_< false > {};


/////////////////////////////////////////////////////////////////////////////////////////
/// \name view, const_view
/// \brief Get an image view from an subchroma_image
/// \ingroup ImageModel
/// \brief Returns the non-constant-pixel view of an image
/////////////////////////////////////////////////////////////////////////////////////////
template< typename Pixel
        , typename Factors
        >
inline
const typename subchroma_image< Pixel, Factors >::view_t& view( subchroma_image< Pixel, Factors >& img )
{
    return img._view;
}

template< typename Pixel
        , typename Factors
        >
inline
const typename subchroma_image< Pixel, Factors >::const_view_t const_view( subchroma_image< Pixel, Factors >& img )
{
    return static_cast< const typename subchroma_image< Pixel, Factors >::const_view_t>( img._view );
}

/////////////////////////////////////////////////////////////////////////////////////////
/// \ingroup ImageViewSTLAlgorithmsFillPixels
/// \brief std::fill for subchroma_image views
/////////////////////////////////////////////////////////////////////////////////////////
template< typename Locator
        , typename Factors
        , typename Pixel
        >
void fill_pixels( const subchroma_image_view< Locator, Factors >& view
                , const Pixel&                                    value
                )
{
    typedef typename subchroma_image< Pixel, Factors >::plane_view_t::value_type channel_t;

    fill_pixels( view.y_plane_view(), channel_t( at_c< 0 >( value )));
    fill_pixels( view.v_plane_view(), channel_t( at_c< 1 >( value )));
    fill_pixels( view.u_plane_view(), channel_t( at_c< 2 >( value )));
}

/////////////////////////////////////////////////////////////////////////////////////////
/// \ingroup ImageViewConstructors
/// \brief Creates a subchroma view from a raw memory
/////////////////////////////////////////////////////////////////////////////////////////
template< typename Pixel
        , typename Factors
        >
typename subchroma_image< Pixel
                        , Factors
                        >::view_t subchroma_view( std::size_t    y_width
                                                , std::size_t    y_height
                                                , unsigned char* y_base
                                                )
{
    typedef Scaling_Factors< mpl::at_c< Factors, 0 >::type::value
                           , mpl::at_c< Factors, 1 >::type::value
                           , mpl::at_c< Factors, 2 >::type::value
                           > scaling_factors_t;

    std::size_t y_channel_size = 1;
    std::size_t u_channel_size = 1;

    unsigned char* u_base = y_base + ( y_width  * y_height * y_channel_size );
    unsigned char* v_base = u_base + ( y_width  / scaling_factors_t::ss_X )
                                   * u_channel_size;

    typedef subchroma_image< Pixel, Factors >::plane_view_t plane_view_t;

    plane_view_t y_plane = interleaved_view( y_width
                                           , y_height
                                           , (plane_view_t::value_type*) y_base // pixels
                                           , y_width                            // rowsize_in_bytes
                                           );

    plane_view_t v_plane = interleaved_view( y_width  / scaling_factors_t::ss_X
                                           , y_height / scaling_factors_t::ss_Y
                                           , (plane_view_t::value_type*) v_base // pixels
                                           , y_width                            // rowsize_in_bytes
                                           );

    plane_view_t u_plane = interleaved_view( y_width  / scaling_factors_t::ss_X
                                           , y_height / scaling_factors_t::ss_Y
                                           , (plane_view_t::value_type*) u_base // pixels
                                           , y_width                            // rowsize_in_bytes
                                           );

    typedef subchroma_image_deref_fn< typename subchroma_image< Pixel
                                                              , Factors
                                                              >::pixel_locator_t
                                    , Factors
                                    > defer_fn_t;

    defer_fn_t deref_fn( y_plane.xy_at( 0, 0 )
                       , v_plane.xy_at( 0, 0 )
                       , u_plane.xy_at( 0, 0 )
                       );


    typedef subchroma_image< Pixel
                           , Factors
                           >::locator_t locator_t;

    locator_t locator( point_t( 0, 0 ) // p
                     , point_t( 1, 1 ) // step
                     , deref_fn
                     );

    typedef subchroma_image< Pixel
                           , Factors
                           >::view_t view_t;

    return view_t( point_t(                           y_width,                           y_height )
                 , point_t( y_width / scaling_factors_t::ss_X, y_height / scaling_factors_t::ss_Y )
                 , point_t( y_width / scaling_factors_t::ss_X, y_height / scaling_factors_t::ss_Y )
                 , locator
                 );
}

} // namespace gil
} // namespace boost

#endif
