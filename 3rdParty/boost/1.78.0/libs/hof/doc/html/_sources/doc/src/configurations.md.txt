<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Configurations
==============

There are several configuration macros that control the behavior of Boost.HigherOrderFunctions library.

```eval_rst
+-----------------------------------------+--------------------------------------------------------------------------------+
| Name                                    | Description                                                                    |
+=========================================+================================================================================+
| ``BOOST_HOF_CHECK_UNPACK_SEQUENCE``     | Unpack has extra checks to ensure that the function will be invoked with the   |
|                                         | sequence. This extra check can help improve error reporting but it can slow    |
|                                         | down compilation. This is enabled by default.                                  |
+-----------------------------------------+--------------------------------------------------------------------------------+
| ``BOOST_HOF_NO_EXPRESSION_SFINAE``      | This controls whether the library will use expression SFINAE to detect the     |
|                                         | callability of functions. On MSVC, this is enabled by default, since it does   |
|                                         | not have full support for expression SFINAE.                                   |
+-----------------------------------------+--------------------------------------------------------------------------------+
| ``BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH`` | Because C++ instantiates `constexpr` functions eagerly, recursion with         |
|                                         | `constexpr` functions can cause the compiler to reach its internal limits. The |
|                                         | setting is used by the library to set a limit on recursion depth to avoid      |
|                                         | infinite template instantiations. The default is 16, but increasing the limit  |
|                                         | can increase compile times.                                                    |
+-----------------------------------------+--------------------------------------------------------------------------------+
```
