# Contributing to Boost.Histogram

## Star the project

If you like Boost.Histogram, please star the project on Github! We want Boost.Histogram to be the best histogram library out there. If you give it a star, it becomes more visible and will gain more users. More users mean more user feedback to make the library even better.

## Support

Feel free to ask questions on https://gitter.im/boostorg/histogram.

## Reporting Issues

We value your feedback about issues you encounter. The more information you provide the easier it is for developers to resolve the problem.

Issues should be reported to the [issue tracker](
https://github.com/boostorg/histogram/issues?state=open).

Issues can also be used to submit feature requests.

Don't be shy: if you are friendly, we are friendly!

## Submitting Pull Requests

Fork the main repository. Base your changes on the `develop` branch. Make a new branch for any feature or bug-fix in your fork. Start developing. You can start a pull request when you feel the change is ready for review.

Please rebase your branch to the original `develop` branch before submitting (which may have diverged from your fork in the meantime).

For general advice on how to set up the Boost project for development, see the Getting Started section on
https://github.com/boostorg/wiki/wiki.

To build the documentation, you need to install a few extra things, see
https://www.boost.org/doc/libs/1_74_0/doc/html/quickbook/install.html.

## Running Tests

### With b2

Boost comes with a build system called `b2`, which is the most efficient way to run the develop-test cycle. It takes a few extra steps and some reading to set up, but the payoff is worth it. If you followed the advice from the previous section, you should be all set up to run the tests from the Boost Histogram project folder with `b2 cxxstd=latest warnings-as-errors=on test`. You can also test the examples by executing `b2 cxxstd=latest examples`. To make the tests complete faster, you can use the option `-j4` (or another number) to build in parallel.

### With cmake

Alternatively, you can build and test Boost Histogram with `cmake`. This does not require setting up all of Boost, just a checkout of Boost Histogram. It requires very little setup, but it is not as efficient for development as using `b2`. Anyway, to use `cmake`, you do in the project folder
```sh
mkdir build
cd build
cmake ..
ctest -C Debug --output-on-failure
```

### Reporting failures

Please report any tests failures to the issue tracker along with the test output and information on your system:

* platform (Linux, Windows, OSX, ...)
* compiler and version

### Test coverage

Boost.Histogram maintains 100% line coverage. Coverage is automatically checked by CI. To generate a report locally, you need to build the code with gcc-8 and coverage instrumentation enabled, do `b2 toolset=gcc-8 cxxstd=latest coverage=on test`. To generate the coverage report, run `tools/cov.sh` from the project root directory of Boost.Histogram. This will generate a new folder `coverage-report` with a HTML report. Open `coverage-report/index.html` in a browser.

Notes: Generating coverage data is very fickle. You need to use gcc-5 or gcc-8 and a matching version of gcov, other gcc versions (6, 7, 9) are known to be broken or are not supported by lcov, which is used to process the raw coverage data. Generating coverage data with clang and llvm-cov is not supported by lcov. The best results are obtained with gcc-5. gcc-8 is known to report lines as missed which are impossible to miss.

## Coding Style

Follow the [Boost Library Requirements and Guidelines](https://www.boost.org/development/requirements.html) and the established style in Boost.Histogram.

### Code formatting

Using `clang-format -style=file` is recommended, which should pick up the `.clang-format` file of the project. All names are written with small letters and `_`. Template parameters are capitalized and in camel-case.

### Documentation

Doxygen comments should be added for all user-facing functions and methods. Implementation details are not documented (everything in the `boost::histogram::detail` namespace is an implementation detail that can change at any time).
