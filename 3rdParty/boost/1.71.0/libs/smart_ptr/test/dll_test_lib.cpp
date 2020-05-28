// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/config.hpp>
#include <memory>

#if defined(DLL_TEST_DYN_LINK)
# define EXPORT BOOST_SYMBOL_EXPORT
#else
# define EXPORT
#endif

EXPORT boost::shared_ptr<int> dll_test_41()
{
    return boost::shared_ptr<int>( new int( 41 ) );
}

EXPORT boost::shared_ptr<int> dll_test_42()
{
    return boost::make_shared<int>( 42 );
}

EXPORT boost::shared_ptr<int> dll_test_43()
{
    return boost::allocate_shared<int>( std::allocator<int>(), 43 );
}

EXPORT boost::shared_ptr<int[]> dll_test_44()
{
    return boost::make_shared<int[1]>( 44 );
}

EXPORT boost::shared_ptr<int[]> dll_test_45()
{
    return boost::allocate_shared<int[1]>( std::allocator<int>(), 45 );
}
