//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPlatform
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/platform.hpp>
#include <boost/compute/system.hpp>

BOOST_AUTO_TEST_CASE(platform_id)
{
    boost::compute::platform platform =
        boost::compute::system::platforms().front();

    boost::compute::platform platform_copy(platform.id());

    BOOST_CHECK(platform == platform_copy);
    BOOST_CHECK(platform.id() == platform_copy.id());
}

BOOST_AUTO_TEST_CASE(platform_supports_extension)
{
    boost::compute::platform platform =
        boost::compute::system::platforms().front();

    std::string extensions = platform.get_info<CL_PLATFORM_EXTENSIONS>();
    if(extensions.empty()){
        std::cerr << "platform doesn't support any extensions" << std::endl;
        return;
    }

    size_t space = extensions.find(' ');
    std::string first_extension = extensions.substr(0, space);
    BOOST_CHECK(platform.supports_extension(first_extension) == true);
    BOOST_CHECK(platform.supports_extension("invalid_extension_name") == false);
}
