// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/is_error_code_enum.hpp>
#include <boost/core/lightweight_test.hpp>

enum error
{
    success = 0,
    e1,
    e2,
    e3
};

namespace boost
{
namespace system
{

template<> struct is_error_code_enum< ::error >
{
    static const bool value = true;
};

}
}

boost::system::error_code make_error_code( ::error e );

enum not_error
{
};

int main()
{
    BOOST_TEST( boost::system::is_error_code_enum< ::error >::value );
    BOOST_TEST( !boost::system::is_error_code_enum< ::not_error >::value );

    return boost::report_errors();
}
