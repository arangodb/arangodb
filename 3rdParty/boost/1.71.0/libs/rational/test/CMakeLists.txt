# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Rational is currently experimental at best
#       and the interface is likely to change in the future

# NOTE: Boost::test not yet available, so we can't run the regular test
# add_executable( test_boost_rational_test rational_test.cpp )
# target_link_libraries( test_boost_rational_test
#     PUBLIC
#         Boost::rational
#         Boost::mpl
#         Boost::test
# )
# add_test( NAME test_boost_rational_test COMMAND test_boost_rational_test )

add_executable( test_boost_rational_constexpr_test constexpr_test.cpp )
target_link_libraries( test_boost_rational_constexpr_test
    PUBLIC
        Boost::rational
)

add_test( NAME test_boost_rational_constexpr_test COMMAND test_boost_rational_constexpr_test )

