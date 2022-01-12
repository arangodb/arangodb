<!--
  Copyright Hans Dembinski 2016 - 2019.
  Distributed under the Boost Software License, Version 1.0.
  (See accompanying file LICENSE_1_0.txt or copy at
  https://www.boost.org/LICENSE_1_0.txt)
-->

![](doc/logo/color.svg)

**Multi-dimensional generalised histograms with convenient interface**

Coded with ❤. Powered by the [Boost community](https://www.boost.org) and the [Scikit-HEP Project](http://scikit-hep.org). Licensed under the [Boost Software License](http://www.boost.org/LICENSE_1_0.txt).

**Supported compiler versions** gcc >= 5.5, clang >= 3.8, msvc >= 14.1
**Supported C++ versions** 14, 17, 20

Branch  | Linux, OSX, Windows    | Coverage
------- | ---------------------- | --------
develop | ![Fast](https://github.com/boostorg/histogram/workflows/Fast/badge.svg?branch=develop) | [![Coveralls](https://coveralls.io/repos/github/boostorg/histogram/badge.svg?branch=develop)](https://coveralls.io/github/boostorg/histogram?branch=develop)
master  | ![Fast](https://github.com/boostorg/histogram/workflows/Fast/badge.svg?branch=master) | [![Coveralls](https://coveralls.io/repos/github/boostorg/histogram/badge.svg?branch=master)](https://coveralls.io/github/boostorg/histogram?branch=master)

Boost.Histogram is a very fast state-of-the-art multi-dimensional generalised [histogram](https://en.wikipedia.org/wiki/Histogram) class for the beginner and expert alike.

* Header-only
* Easy to use, easy to customise
* Designed for performance
* Just count or use accumulators to compute means, minimum, maximum, ...
* Supports multi-threading and restricted environments (no heap allocation, exceptions or RTTI)
* Open hacker-friendly modular design
* Has [official Python bindings](https://github.com/scikit-hep/boost-histogram)

Check out the [full documentation](https://www.boost.org/doc/libs/master/libs/histogram/doc/html/index.html).

💡 Boost.Histogram is a mature library with 100 % of code lines covered by unit tests, is benchmarked for performance, and has extensive documentation. If you still find some issue or find the documentation lacking, tell us about it by [submitting an issue](https://github.com/boostorg/histogram/issues). Chat with us on the [Boost channel on Slack](https://cpplang.slack.com) and [Gitter](https://gitter.im/boostorg/histogram).

## Code examples

The following stripped-down example was taken from the [Getting started](https://www.boost.org/doc/libs/master/libs/histogram/doc/html/histogram/getting_started.html) section in the documentation. Have a look into the docs to see the full version with comments and more examples.

Example: Make and fill a 1d-histogram ([try it live on Wandbox](https://wandbox.org/permlink/NSM2ZiDyntUi6RDC)). The core of this example [compiles into 53 lines of assembly code](https://godbolt.org/z/632yzE).

```cpp
#include <boost/histogram.hpp>
#include <boost/format.hpp> // used here for printing
#include <iostream>

int main() {
    using namespace boost::histogram;

    // make 1d histogram with 4 regular bins from 0 to 2
    auto h = make_histogram( axis::regular<>(4, 0.0, 2.0) );

    // push some values into the histogram
    for (auto&& value : { 0.4, 1.1, 0.3, 1.7, 10. })
      h(value);

    // iterate over bins
    for (auto&& x : indexed(h)) {
      std::cout << boost::format("bin %i [ %.1f, %.1f ): %i\n")
        % x.index() % x.bin().lower() % x.bin().upper() % *x;
    }

    std::cout << std::flush;

    /* program output:

    bin 0 [ 0.0, 0.5 ): 2
    bin 1 [ 0.5, 1.0 ): 0
    bin 2 [ 1.0, 1.5 ): 1
    bin 3 [ 1.5, 2.0 ): 1
    */
}
```

## Features

* Extremely customisable multi-dimensional histogram
* Simple, convenient, STL and Boost-compatible interface
* Counters with high dynamic range, cannot overflow or be capped [[1]](#note1)
* Better performance than other libraries (see benchmarks for details)
* Efficient use of memory [[1]](#note1)
* Hand-crafted C++17 deduction guides for axes and histogram types
* Support for custom axis types: define how input values should map to indices
* Support for under-/overflow bins (can be disabled individually to reduce memory consumption)
* Support for axes that grow automatically with input values [[2]](#note2)
* Support for weighted increments
* Support for profiles and more generally, user-defined accumulators in cells [[3]](#note3)
* Support for completely stack-based histograms
* Support for compilation without exceptions or RTTI [[4]](#note4)
* Support for adding, subtracting, multiplying, dividing, and scaling histograms
* Support for custom allocators
* Support for programming with units [[5]](#note5)
* Optional serialization based on [Boost.Serialization](https://www.boost.org/doc/libs/release/libs/serialization/)

<b id="note1">Note 1</b> In the standard configuration, if you don't use weighted increments. The counter capacity is increased dynamically as the cell counts grow. When even the largest plain integral type would overflow, the storage switches to a multiprecision integer similar to those in [Boost.Multiprecision](https://www.boost.org/doc/libs/release/libs/multiprecision/), which is only limited by available memory.

<b id="note2">Note 2</b> An axis can be configured to grow when a value is encountered that is outside of its range. It then grows new bins towards this value so that the value ends up in the new highest or lowest bin.

<b id="note3">Note 3</b> The histogram can be configured to hold an arbitrary accumulator in each cell instead of a simple counter. Extra values can be passed to the histogram, for example, to compute the mean and variance of values which fall into the same cell. This feature can be used to calculate variance estimates for each cell, which are useful when you need to fit a statistical model to the cell values.

<b id="note4">Note 4</b> The library throws exceptions when exceptions are enabled. When exceptions are disabled, a user-defined exception handler is called instead upon a throw and the program terminates, see [boost::throw_exception](https://www.boost.org/doc/libs/master/libs/exception/doc/throw_exception.html) for details. Disabling exceptions improves performance by 10 % to 20 % in benchmarks. The library does not use RTTI (only CTTI) so disabling it has no effect.

<b id="note5">Note 5</b> Builtin axis types can be configured to only accept dimensional quantities, like those from [Boost.Units](https://www.boost.org/doc/libs/release/libs/units/). This means you get a useful error if you accidentally try to fill a length where the histogram axis expects a time, for example.

## Benchmarks

Boost.Histogram is more flexible and faster than other C/C++ libraries. It was compared to:
 - [GNU Scientific Library](https://www.gnu.org/software/gsl)
 - [ROOT framework from CERN](https://root.cern.ch)

Details on the benchmark are given in the [documentation](https://www.boost.org/doc/libs/develop/libs/histogram/doc/html/histogram/benchmarks.html).

## What users say

**John Buonagurio** | Manager at [**E<sup><i>x</i></sup>ponent<sup>&reg;</sup>**](https://www.exponent.com)

*"I just wanted to say 'thanks' for your awesome Histogram library. I'm working on a software package for processing meteorology data and I'm using it to generate wind roses with the help of Qt and QwtPolar. Looks like you thought of just about everything here &ndash; the circular axis type was practically designed for this application, everything 'just worked'."*
