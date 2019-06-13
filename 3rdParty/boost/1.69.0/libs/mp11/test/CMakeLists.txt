# Copyright 2018 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

boost_test_jamfile(FILE Jamfile LIBRARIES Boost::mp11 Boost::core)
boost_test(SOURCES check_cmake_version.cpp ARGUMENTS ${PROJECT_VERSION} LIBRARIES Boost::core Boost::config)
