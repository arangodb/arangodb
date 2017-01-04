# Boost.Compute #

[![Build Status](https://travis-ci.org/boostorg/compute.png?branch=master)]
 (https://travis-ci.org/boostorg/compute)
 [![Coverage Status](https://coveralls.io/repos/boostorg/compute/badge.png?branch=master)]
 (https://coveralls.io/r/boostorg/compute)

Boost.Compute is a GPU/parallel-computing library for C++ based on OpenCL.

The core library is a thin C++ wrapper over the OpenCL API and provides
access to compute devices, contexts, command queues and memory buffers.

On top of the core library is a generic, STL-like interface providing common
algorithms (e.g. `transform()`, `accumulate()`, `sort()`) along with common
containers (e.g. `vector<T>`, `flat_set<T>`). It also features a number of
extensions including parallel-computing algorithms (e.g. `exclusive_scan()`,
`scatter()`, `reduce()`) and a number of fancy iterators (e.g.
`transform_iterator<>`, `permutation_iterator<>`, `zip_iterator<>`).

The full documentation is available at http://boostorg.github.io/compute/.

## Example ##

The following example shows how to sort a vector of floats on the GPU:

```c++
#include <vector>
#include <algorithm>
#include <boost/compute.hpp>

namespace compute = boost::compute;

int main()
{
    // get the default compute device
    compute::device gpu = compute::system::default_device();

    // create a compute context and command queue
    compute::context ctx(gpu);
    compute::command_queue queue(ctx, gpu);

    // generate random numbers on the host
    std::vector<float> host_vector(1000000);
    std::generate(host_vector.begin(), host_vector.end(), rand);

    // create vector on the device
    compute::vector<float> device_vector(1000000, ctx);

    // copy data to the device
    compute::copy(
        host_vector.begin(), host_vector.end(), device_vector.begin(), queue
    );

    // sort data on the device
    compute::sort(
        device_vector.begin(), device_vector.end(), queue
    );

    // copy data back to the host
    compute::copy(
        device_vector.begin(), device_vector.end(), host_vector.begin(), queue
    );

    return 0;
}
```

Boost.Compute is a header-only library, so no linking is required. The example
above can be compiled with:

`g++ -I/path/to/compute/include sort.cpp -lOpenCL`

More examples can be found in the [tutorial](
http://boostorg.github.io/compute/boost_compute/tutorial.html) and under the
[examples](https://github.com/boostorg/compute/tree/master/example) directory.

## Support ##
Questions about the library (both usage and development) can be posted to the
[mailing list](https://groups.google.com/forum/#!forum/boost-compute).

Bugs and feature requests can be reported through the [issue tracker](
https://github.com/boostorg/compute/issues?state=open).

Also feel free to send me an email with any problems, questions, or feedback.

## Help Wanted ##
The Boost.Compute project is currently looking for additional developers with
interest in parallel computing.

Please send an email to Kyle Lutz (kyle.r.lutz@gmail.com) for more information.
