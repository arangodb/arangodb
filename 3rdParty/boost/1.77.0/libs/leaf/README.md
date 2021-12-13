# LEAF

> A lightweight error handling library for C++11.

## Documentation

https://boostorg.github.io/leaf/

## Features

* Small single-header format, **no dependencies**.
* Designed for maximum efficiency ("happy" path and "sad" path).
* No dynamic memory allocations, even with heavy payloads.
* O(1) transport of arbitrary error types (independent of call stack depth).
* Can be used with or without exception handling.
* Support for multi-thread programming.

## Support

* [cpplang on Slack](https://Cpplang.slack.com) (use the `#boost` channel)
* [Boost Users Mailing List](https://lists.boost.org/mailman/listinfo.cgi/boost-users)
* [Boost Developers Mailing List](https://lists.boost.org/mailman/listinfo.cgi/boost)

## Distribution

Besides GitHub, there are two other distribution channels:

* LEAF is included in official [Boost](https://www.boost.org/) releases, starting with Boost 1.75.
* For maximum portability, the library is also available in single-header format: simply download [leaf.hpp](https://boostorg.github.io/leaf/leaf.hpp) (direct download link).

Copyright (C) 2018-2021 Emil Dotchevski. Distributed under the http://www.boost.org/LICENSE_1_0.txt[Boost Software License, Version 1.0].
