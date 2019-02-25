//
// Copyright 2007-2008 Christian Henning, Andreas Pokorny
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_CONVERSION_POLICIES_HPP
#define BOOST_GIL_IO_CONVERSION_POLICIES_HPP

#include <boost/gil/image_view_factory.hpp>
#include <boost/gil/io/error.hpp>

#include <algorithm>
#include <iterator>

namespace boost{namespace gil{ namespace detail {

struct read_and_no_convert
{
public:
    typedef void* color_converter_type;

    template< typename InIterator
            , typename OutIterator
            >
    void read( const InIterator& /* begin */
             , const InIterator& /* end   */
             , OutIterator       /* out   */
             , typename disable_if< typename pixels_are_compatible< typename std::iterator_traits<InIterator>::value_type
                                                                  , typename std::iterator_traits<OutIterator>::value_type
                                                                  >::type
                                  >::type* /* ptr */ = 0
             )
    {
        io_error( "Data cannot be copied because the pixels are incompatible." );
    }

    template< typename InIterator
            , typename OutIterator
            >
    void read( const InIterator& begin
             , const InIterator& end
             , OutIterator       out
             , typename enable_if< typename pixels_are_compatible< typename std::iterator_traits<InIterator>::value_type
                                                                 , typename std::iterator_traits<OutIterator>::value_type
                                                                 >::type
                                 >::type* /* ptr */ = 0
             )
    {
        std::copy( begin
                 , end
                 , out
                 );
    }

};

template<typename CC>
struct read_and_convert
{
public:
    typedef CC color_converter_type;
    CC _cc;

    read_and_convert()
    {}

    read_and_convert( const color_converter_type& cc )
    : _cc( cc )
    {}

    template< typename InIterator
            , typename OutIterator
            >
    void read( const InIterator& begin
             , const InIterator& end
             , OutIterator       out
             )
    {
        typedef color_convert_deref_fn< typename std::iterator_traits<InIterator>::reference
                                      , typename std::iterator_traits<OutIterator>::value_type //reference?
                                      , CC
                                      > deref_t;

        std::transform( begin
                      , end
                      , out
                      , deref_t( _cc )
                      );
    }
};

/// is_read_only metafunction
/// \brief Determines if reader type is read only ( no conversion ).
template< typename Conversion_Policy >
struct is_read_only : mpl::false_ {};

template<>
struct is_read_only< detail::read_and_no_convert > : mpl::true_ {};

} // namespace detail
} // namespace gil
} // namespace boost

#endif
