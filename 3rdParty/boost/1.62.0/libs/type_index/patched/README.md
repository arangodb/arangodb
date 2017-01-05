patches for Boost libraries to work without RTTI 
==========

Here are the patches that are TESTED and work well with RTTI disabled and enabled. 
Patches add tests for some of the libraries to make sure that library compile and work without RTTI.

Patches remove duplicate code, improve output, allow compilation with RTTI off...

Libraries Boost.Graph, Boost.XPressive, Boost.PropertyMap and others may also benefit from TypeIndex library.
