# Contributing to Boost.Compute #

## Reporting Issues ##

We value your feedback about issues you encounter. The more information you
provide the easier it is for developers to resolve the problem.

Issues should be reported to the [issue tracker](
https://github.com/boostorg/compute/issues?state=open).

Issues can also be used to submit feature requests.

## Submitting Pull Requests ##

Base your changes on `master` but submit your pull-request to `develop`. This
can be changed by clicking the "Edit" button on the pull-request page. The
develop branch is used for integration and testing of changes before merging
into the stable `master` branch.

Please try to rebase your changes on the current master branch before
submitting. This keeps the git history cleaner and easier to understand.

## Coding Style ##

* Indentation is four-spaces (not tabs)
* Try to keep line-length under 80 characters
* Follow the STL/Boost naming conventions (e.g. lower case with underscores)
* When in doubt, match the style of existing code
* Otherwise, do whatever you want

Also see the [Boost Library Requirements]
(http://www.boost.org/development/requirements.html)).

## Running Tests ##

To build the tests you must enable the `BOOST_COMPUTE_BUILD_TESTS` option in
`cmake`. The tests can be run by executing the `ctest` command from the build
directory.

Please report any tests failures to the issue tracker along with the test
output and information on your system and compute device.

## Support ##

Feel free to send an email to kyle.r.lutz@gmail.com with any problems or
questions.
