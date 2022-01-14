// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/generic_category.hpp>
#include <boost/core/lightweight_test.hpp>

// Tests whether generic_category() is functional when only
// generic_category.hpp is included

namespace sys = boost::system;

int main()
{
    sys::error_category const & cat = sys::generic_category();

    BOOST_TEST_CSTR_EQ( cat.name(), "generic" );

    return boost::report_errors();
}
