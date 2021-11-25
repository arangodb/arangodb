/*==============================================================================
    Copyright (c) 2006 Peter Dimov
    Copyright (c) 2014 Agustin Berge
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/config.hpp>

#if defined(BOOST_MSVC)
#pragma warning(disable: 4786)  // identifier truncated in debug info
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4514)  // unreferenced inline removed
#endif

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/ref.hpp>

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1300)
#pragma warning(push, 3)
#endif

#include <iostream>

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1300)
#pragma warning(pop)
#endif

#include <boost/detail/lightweight_test.hpp>

//

struct Z
{
    int m;
};

void member_data_test()
{
    using boost::phoenix::bind;

    Z z = { 17041 };
    Z * pz = &z;

    bind<void>( &Z::m, _1 )( z );
    bind<void>( &Z::m, _1 )( pz );

    bind<void>( &Z::m, z )();
    bind<void>( &Z::m, pz )();
    bind<void>( &Z::m, boost::ref(z) )();


    Z const cz = z;
    Z const * pcz = &cz;

    bind<void>( &Z::m, _1 )( cz );
    bind<void>( &Z::m, _1 )( pcz );

    bind<void>( &Z::m, cz )();
    bind<void>( &Z::m, pcz )();
    bind<void>( &Z::m, boost::ref(cz) )();
}

int main()
{
    member_data_test();

    return boost::report_errors();
}
