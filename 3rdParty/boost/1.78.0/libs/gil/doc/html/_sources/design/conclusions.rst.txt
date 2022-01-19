Conclusions
===========

.. contents::
   :local:
   :depth: 2

The Generic Image Library is designed with the following five goals in mind:

Generality
----------

Abstracts image representations from algorithms on images.
It allows for writing code once and have it work for any image type.

Performance
-----------

Speed has been instrumental to the design of the library.
The generic algorithms provided in the library are in many cases comparable
in speed to hand-coding the algorithm for a specific image type.

Flexibility
-----------

Compile-type parameter resolution results in faster code, but severely limits
code flexibility. The library allows for any image parameter to be specified
at run time, at a minor performance cost.

Extensibility
-------------

Virtually every construct in GIL can be extended - new channel types,
color spaces, layouts, iterators, locators, image views and images
can be provided by modeling the corresponding GIL concepts.

Compatibility
-------------

The library is designed as an STL complement.
Generic STL algorithms can be used for pixel manipulation, and they are
specifically targeted for optimization. The library works with existing
raw pixel data from another image library.
