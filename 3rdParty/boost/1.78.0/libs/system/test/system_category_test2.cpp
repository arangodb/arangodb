// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/system_category.hpp>
#include <boost/core/lightweight_test.hpp>

// Tests whether system_category() is functional when only
// system_category.hpp is included

namespace sys = boost::system;

int main()
{
    sys::error_category const & cat = sys::system_category();

    BOOST_TEST_CSTR_EQ( cat.name(), "system" );

    return boost::report_errors();
}
