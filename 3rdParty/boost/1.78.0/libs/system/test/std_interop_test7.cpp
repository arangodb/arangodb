// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config.hpp>
#include <cerrno>

#if !defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

BOOST_PRAGMA_MESSAGE( "BOOST_SYSTEM_HAS_SYSTEM_ERROR not defined, test will be skipped" )
int main() {}

#else

#include <system_error>

int main()
{
    {
        boost::system::error_code e1 = make_error_code( boost::system::errc::bad_address );

        BOOST_TEST( e1 == boost::system::errc::bad_address );
        BOOST_TEST_NOT( e1 != boost::system::errc::bad_address );

        BOOST_TEST( e1 == std::errc::bad_address );
        BOOST_TEST_NOT( e1 != std::errc::bad_address );
    }

    {
        boost::system::error_code e1 = make_error_code( std::errc::bad_address );

        BOOST_TEST( e1 == boost::system::errc::bad_address );
        BOOST_TEST_NOT( e1 != boost::system::errc::bad_address );

#if defined(BOOST_GCC) && BOOST_GCC >= 40800 && BOOST_GCC < 50000

// fails on g++ 4.8.5 and g++ 4.9.4 from ubuntu-toolchain-r-test for unknown reasons
// works on the system g++ 4.8.4 on Trusty and the system g++ 4.8.5 on CentOS 7

#else
        BOOST_TEST( e1 == std::errc::bad_address );
        BOOST_TEST_NOT( e1 != std::errc::bad_address );
#endif
    }

    return boost::report_errors();
}

#endif
