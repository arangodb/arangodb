# QVM

> A generic C++ library for working with `Q`uaternions, `V`ectors and `M`atrices.

## Documentation

https://boostorg.github.io/qvm/

## Features

* Emphasis on 2, 3 and 4-dimensional operations needed in graphics, video games and simulation applications.
* Free function templates operate on any compatible user-defined Quaternion, Vector or Matrix type.
* Enables Quaternion, Vector and Matrix types from different libraries to be safely mixed in the same expression.
* Type-safe mapping between compatible lvalue types with no temporary objects; f.ex. transpose remaps the access to the elements, rather than transforming the matrix.
* Requires only {CPP}03.
* Zero dependencies.

## Support

* [cpplang on Slack](https://Cpplang.slack.com) (use the `#boost` channel)
* [Boost Users Mailing List](https://lists.boost.org/mailman/listinfo.cgi/boost-users)
* [Boost Developers Mailing List](https://lists.boost.org/mailman/listinfo.cgi/boost)

## Distribution

Besides GitHub, there are two other distribution channels:

* QVM is included in official [Boost](https://www.boost.org/) releases.
* For maximum portability, the library is also available in single-header format, in two variants (direct download links):
	* [qvm.hpp](https://boostorg.github.io/qvm/qvm.hpp): single header containing the complete QVM source, including the complete set of swizzling overloads.
	* [qvm_lite.hpp](https://boostorg.github.io/qvm/qvm_lite.hpp): single header containing everything except for the swizzling overloads.

Copyright (C) 2008-2021 Emil Dotchevski. Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).
