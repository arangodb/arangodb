# Copyright Louis Dionne 2013-2017
# Copyright Markus J. Weber 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module checks whether the current compiler is supported, and
# provides friendly hints to the user.

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "3.5.0")
        message(WARNING "
    ### You appear to be using Clang ${CMAKE_CXX_COMPILER_VERSION}, which is known
    ### to be unable to compile Hana. Consider switching to
    ### Clang >= 3.5.0. If it is already installed on your
    ### system, you can tell CMake about it with
    ###
    ###     cmake .. -DCMAKE_CXX_COMPILER=/path/to/clang
        ")
    endif()

    if (MSVC)
        if(${MSVC_VERSION} LESS 1900)
            message(WARNING "
    ###
    ### We detected that you were using Clang for Windows with a
    ### -fms-compatibility-version parameter lower than 19. Only
    ### -fms-compatibility-version=19 and above are supported by
    ### Hana for lack of proper C++14 support prior for versions
    ### below that.
    ###
    ### If this diagnostic is wrong and you are not using
    ### -fms-compatibility-version, please file an issue at
    ### https://github.com/boostorg/hana/issues.
    ###
            ")
        endif()
    endif()
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "6.1.0")
        message(WARNING "
    ### You appear to be using Apple's Clang ${CMAKE_CXX_COMPILER_VERSION}, which is
    ### shipped with Xcode < 6.3. Unfortunately, only Apple's Clang
    ### >= 6.1.0 (shipped with Xcode >= 6.3) can compile Hana.
    ### You should consider updating to Xcode >= 6.3 (requires Yosemite)
    ### or using a non-Apple Clang >= 3.5.0, which can be installed via
    ### Homebrew with
    ###
    ###     brew install llvm --with-clang
    ###
    ### You can then tell CMake to use that non-system Clang with
    ###
    ###     cmake .. -DCMAKE_CXX_COMPILER=/path/to/clang
        ")
    endif()
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "6.0.0")
        message(WARNING "
    ### You appear to be using GCC ${CMAKE_CXX_COMPILER_VERSION}, which is known to be
    ### unable to compile Hana. Only GCC >= 6.0.0 is supported.
    ### Consider using a more recent GCC or switching to Clang.
    ### If a more recent compiler is already installed on your
    ### system, you can tell CMake to use it with
    ###
    ###     cmake .. -DCMAKE_CXX_COMPILER=/path/to/compiler
        ")
    endif()
elseif (MSVC)
    message(WARNING "
    ### Using the native Microsoft compiler (MSVC) is not supported for lack
    ### of proper C++14 support. However, you can install pre-built Clang for
    ### Windows binaries (with Visual Studio integration if desired) at
    ### http://llvm.org/releases/download.html.
    ###
    ### More information about how to set up Hana with Clang for Windows is
    ### available on Hana's wiki at http://git.io/vBYIp.
    ")
else()
    message(WARNING "
    ### You appear to be using a compiler that is not yet tested with Hana.
    ### Please tell us whether it successfully compiles or if and how it
    ### fails by opening an issue at https://github.com/boostorg/hana/issues.
    ### Thanks!
    ")
endif()
