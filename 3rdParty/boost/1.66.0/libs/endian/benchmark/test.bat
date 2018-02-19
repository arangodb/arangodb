b2 -a toolset=msvc-14.0 variant=release link=static address-model=64
bin\loop_time_test 1000 >msvc-loop-time.html
msvc-loop-time.html

echo The GCC static build does not work on Windows, probably because of a bjam/b2 bug
b2 -a toolset=gcc-c++11 variant=release link=static address-model=64
bin\loop_time_test 1000 >gcc-loop-time.html
gcc-loop-time.html

rem Copyright Beman Dawes 2015
rem Distributed under the Boost Software License, Version 1.0.
rem See www.boost.org/LICENSE_1_0.txt
