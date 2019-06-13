
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/core/typeinfo.hpp>
#include <boost/config.hpp>

#if defined(LIB_TYPEID_DYN_LINK)
# define EXPORT BOOST_SYMBOL_EXPORT
#else
# define EXPORT
#endif

EXPORT boost::core::typeinfo const & get_typeid_int()
{
    return BOOST_CORE_TYPEID( int );
}
