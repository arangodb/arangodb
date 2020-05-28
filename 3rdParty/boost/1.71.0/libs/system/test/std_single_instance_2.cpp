
// Copyright 2019 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/config.hpp>

#if defined(STD_SINGLE_INSTANCE_DYN_LINK)
# define EXPORT BOOST_SYMBOL_EXPORT
#else
# define EXPORT
#endif

#include <boost/system/error_code.hpp>

#if defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

#include <system_error>

namespace lib2
{

EXPORT std::error_code get_system_code()
{
    return boost::system::error_code( 0, boost::system::system_category() );
}

EXPORT std::error_code get_generic_code()
{
    return boost::system::error_code( 0, boost::system::generic_category() );
}

} // namespace lib2

#else

EXPORT void lib2_f()
{
}

#endif
