
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/function.hpp>
#include <boost/config.hpp>

#if defined(MIXED_CXXSTD_DYN_LINK)
# define EXPORT BOOST_SYMBOL_EXPORT
#else
# define EXPORT
#endif

EXPORT void call_fn_1( boost::function<void()> const & fn )
{
    fn();
}

EXPORT void call_fn_2( boost::function<void(int)> const & fn )
{
    fn( 1 );
}

EXPORT void call_fn_3( boost::function<void(int, int)> const & fn )
{
    fn( 1, 2 );
}

EXPORT void call_fn_4( boost::function0<void> const & fn )
{
    fn();
}

EXPORT void call_fn_5( boost::function1<void, int> const & fn )
{
    fn( 1 );
}

EXPORT void call_fn_6( boost::function2<void, int, int> const & fn )
{
    fn( 1, 2 );
}
