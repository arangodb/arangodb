# Copyright 2021 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

file(GLOB SOURCES "*.cpp")
add_library(boost_math-compile_tests STATIC ${SOURCES})
target_compile_features(boost_math-compile_tests PRIVATE cxx_std_17)
target_link_libraries(boost_math-compile_tests PUBLIC Boost::math)
