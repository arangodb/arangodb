
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>

using namespace boost::system;

static void f( error_code & ec )
{
    ec = error_code();
}

#if defined(_WIN32)
# include <windows.h> // SetErrorMode
#endif

int main()
{
#if defined(_WIN32)

    SetErrorMode( SetErrorMode( 0 ) | SEM_NOGPFAULTERRORBOX );

#endif

    // this should crash
    f( boost::throws() );
}
