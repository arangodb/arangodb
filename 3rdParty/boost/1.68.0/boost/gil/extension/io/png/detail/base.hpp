/*
    Copyright 2012 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

#ifndef BOOST_GIL_EXTENSION_IO_PNG_DETAIL_BASE_HPP
#define BOOST_GIL_EXTENSION_IO_PNG_DETAIL_BASE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2012 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/extension/io/png/tags.hpp>

#include <memory>

namespace boost { namespace gil { namespace detail {

struct png_ptr_wrapper
{
    png_ptr_wrapper()
    : _struct( NULL )
    , _info  ( NULL )
    {}

    png_structp _struct;
    png_infop   _info;
};

///
/// Wrapper for libpng's png_struct and png_info object. Implements value semantics.
///
struct png_struct_info_wrapper
{
protected:

    using png_ptr_t = std::shared_ptr<png_ptr_wrapper>;

protected:

    ///
    /// Default Constructor
    ///
    png_struct_info_wrapper( bool read = true )
    : _png_ptr( new png_ptr_wrapper()
              , ( ( read ) ? png_ptr_read_deleter : png_ptr_write_deleter )
              )
    {}

    png_ptr_wrapper*       get()       { return _png_ptr.get(); }
    const png_ptr_wrapper* get() const { return _png_ptr.get(); }
    
    png_structp       get_struct()       { return get()->_struct; }
    const png_structp get_struct() const { return get()->_struct; }

    png_infop       get_info()       { return get()->_info; }
    const png_infop get_info() const { return get()->_info; }

private:

    static void png_ptr_read_deleter( png_ptr_wrapper* png_ptr )
    {
        if( png_ptr )
        {
            assert( png_ptr->_struct && png_ptr->_info );

            png_destroy_read_struct( &png_ptr->_struct
                                   , &png_ptr->_info
                                   , NULL
                                   );

            delete png_ptr;
            png_ptr = NULL;
        }
    }

    static void png_ptr_write_deleter( png_ptr_wrapper* png_ptr )
    {
        if( png_ptr )
        {
            assert( png_ptr->_struct && png_ptr->_info );

            png_destroy_write_struct( &png_ptr->_struct
                                    , &png_ptr->_info
                                    );

            delete png_ptr;
            png_ptr = NULL;
        }
    }


private:

    png_ptr_t _png_ptr;
};

} // namespace detail
} // namespace gil
} // namespace boost

#endif
