Features and Improvements in ArangoDB 3.5
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.5. ArangoDB 3.5 also contains several bug fixes that are not listed
here.

Internal
--------

We have moved from C++11 to C++14, which allows us to use some of the simplifications,
features and guarantees that this standard has in stock.
To compile ArangoDB from source, a compiler that supports C++14 is now required.

The bundled JEMalloc memory allocator used in ArangoDB release packages has been
upgraded from version 5.0.1 to version 5.1.0.
