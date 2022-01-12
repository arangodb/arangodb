#include <boost/config.hpp>

#if defined(BOOST_MSVC)
#pragma warning(disable: 4786)  // identifier truncated in debug info
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4514)  // unreferenced inline removed
#endif

//
//  bind_void_mf_test.cpp - test for bind<void> with member functions
//
//  Copyright (c) 2008 Peter Dimov
//  Copyright (c) 2014 Agustin Berge
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/bind/bind.hpp>
#include <boost/ref.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::placeholders;

//

struct Z
{
    int m;
};

void member_data_test()
{
    Z z = { 17041 };
    Z * pz = &z;

    boost::bind<void>( &Z::m, _1 )( z );
    boost::bind<void>( &Z::m, _1 )( pz );

    boost::bind<void>( &Z::m, z )();
    boost::bind<void>( &Z::m, pz )();
    boost::bind<void>( &Z::m, boost::ref(z) )();


    Z const cz = z;
    Z const * pcz = &cz;

    boost::bind<void>( &Z::m, _1 )( cz );
    boost::bind<void>( &Z::m, _1 )( pcz );

    boost::bind<void>( &Z::m, cz )();
    boost::bind<void>( &Z::m, pcz )();
    boost::bind<void>( &Z::m, boost::ref(cz) )();
}

int main()
{
    member_data_test();

    return boost::report_errors();
}
