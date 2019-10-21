# How to run the unit tests

This folder contains the unit tests for Boost.Test.

In order to run the unit tests, you first need to create `b2`. Check the documentation of boost 
on how to generate `b2`.

## OSX

Please run the tests in C++11 mode, with the following commands

    cd <boost-root-folder>
    ./bootstrap.sh
    ./b2 headers
    cd libs/test/test
    ../../../b2 -j8 toolset=clang cxxflags="-stdlib=libc++ -std=c++11" linkflags="-stdlib=libc++" 

## Linux

As for OSX, please run the tests in C++11 mode, using the following commands

    cd <boost-root-folder>
    ./bootstrap.sh
    ./b2 headers
    cd libs/test/test
    ../../../b2 cxxflags=-std=c++11

## Windows


### Visual Studio 2017 C++17 mode
To run the tests for Visual Studio 2017 / C++17 mode, use the following commands:

    cd <boost-root-folder>
    call bootstrap.bat
    b2 headers
    cd libs\test\test
    ..\..\..\b2 --abbreviate-paths toolset=msvc-14.1 cxxflags="/std:c++latest"