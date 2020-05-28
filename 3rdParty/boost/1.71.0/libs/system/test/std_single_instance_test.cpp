
// Copyright 2019 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config/helper_macros.hpp>

#if !defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

BOOST_PRAGMA_MESSAGE( "BOOST_SYSTEM_HAS_SYSTEM_ERROR not defined, test will be skipped" )
int main() {}

#elif defined(STD_SINGLE_INSTANCE_SHARED) && defined(__CYGWIN__)

BOOST_PRAGMA_MESSAGE( "Skipping Windows/DLL test, __CYGWIN__" )
int main() {}

#elif defined(STD_SINGLE_INSTANCE_SHARED) && defined(_WIN32) && !defined(_MSC_VER)

BOOST_PRAGMA_MESSAGE( "Skipping Windows/DLL test, no _MSC_VER" )
int main() {}

#elif defined(STD_SINGLE_INSTANCE_SHARED) && defined(_WIN32) && !defined(_CPPLIB_VER)

BOOST_PRAGMA_MESSAGE( "Skipping Windows/DLL test, no _CPPLIB_VER" )
int main() {}

#elif defined(STD_SINGLE_INSTANCE_SHARED) && defined(_WIN32) && (_MSC_VER < 1900 || _MSC_VER >= 2000)

BOOST_PRAGMA_MESSAGE( "Skipping Windows/DLL test, _MSC_VER is " BOOST_STRINGIZE(_MSC_VER) )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>
#include <system_error>

using namespace boost::system;

namespace lib1
{

std::error_code get_system_code();
std::error_code get_generic_code();

} // namespace lib1

namespace lib2
{

std::error_code get_system_code();
std::error_code get_generic_code();

} // namespace lib2

int main()
{
    {
        std::error_code e1 = lib1::get_system_code();
        std::error_code e2 = lib2::get_system_code();

        BOOST_TEST_EQ( e1, e2 );
    }

    {
        std::error_code e1 = lib1::get_generic_code();
        std::error_code e2 = lib2::get_generic_code();

        BOOST_TEST_EQ( e1, e2 );
    }

    return boost::report_errors();
}

#endif
