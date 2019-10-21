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

Don't be shy: if you are friendly, we are friendly! And we care, issues are usually answered within a working day.

## Submitting Pull Requests

Fork the main repository. Base your changes on the `develop` branch. Make a new branch for any feature or bug-fix in your fork. Start developing. You can start a pull request when you feel the change is ready for review.

Please rebase your branch to the original `develop` branch before submitting (which may have diverged from your fork in the meantime).

For general advice on how to set up the Boost project for development, see
https://github.com/boostorg/boost/wiki/Getting-Started.

To build the documentation, you need to install a few extra things, see
https://www.boost.org/doc/libs/1_70_0/doc/html/quickbook/install.html.

## Running Tests

To run the tests from the project folder, do `b2 cxxstd=14 test`. You can also test the examples by executing `b2 cxxstd=14 examples`.

Please report any tests failures to the issue tracker along with the test
output and information on your system:

* platform (Linux, Windows, OSX, ...)
* compiler and version

## Coding Style

Follow the [Boost Library Requirements and Guidelines](https://www.boost.org/development/requirements.html) and the established style in Boost.Histogram.

### Code formatting

Using `clang-format -style=file` is recommended, which should pick up the `.clang-format` file of the project. All names are written with small letters and `_`. Template parameters are capitalized and in camel-case.

### Documentation

Doxygen comments should be added for all user-facing functions and methods. Implementation details are not documented (everything in the `boost::histogram::detail` namespace is an implementation detail that can change at any time).
