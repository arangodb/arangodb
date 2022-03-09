<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Building
========

Boost.HigherOrderFunctions library uses cmake to build. To configure with cmake create a build directory, and run cmake:

    mkdir build
    cd build
    cmake ..

Installing
----------

To install the library just run the `install` target:

    cmake --build . --target install

Tests
-----

The tests can be built and run by using the `check` target:

    cmake --build . --target check

The tests can also be ran using Boost.Build, just copy library to the boost source tree, and then:

    cd test
    b2

Documentation
-------------

The documentation is built using Sphinx. First, install the requirements needed for the documentation using `pip`:

    pip install -r doc/requirements.txt

Then html documentation can be generated using `sphinx-build`:

    sphinx-build -b html doc/ doc/html/

The final docs will be in the `doc/html` folder.

