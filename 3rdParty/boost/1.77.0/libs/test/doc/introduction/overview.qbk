[/
 / Copyright (c) 2003 Boost.Test contributors
 /
 / Distributed under the Boost Software License, Version 1.0. (See accompanying
 / file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 /]

[section:design_rationale Design rationale]

Unit testing tasks arise during many different stages of software development: from initial project
implementation to its maintenance and later revisions. These tasks differ in their complexity and purpose
and accordingly are approached differently by different developers. The wide spectrum of tasks in a problem
domain cause many requirements (sometimes conflicting) to be placed on a unit testing framework. These
include:

* Writing a [link ref_test_module unit test module] should be simple and obvious for new users.
* The framework should allow advanced users to perform non-trivial tests.
* Test module should be able to have many small test cases and developer should be able to group them into
  test suites.
* At the beginning of the development users want to see verbose and descriptive error messages.
* During the regression testing users just want to know if any tests failed.
* For small test modules, their execution time should prevail over compilation time: user don't want to wait a minute
  to compile a test that takes a second to run.
* For long and complex tests users want to be able to see the testing progress.
* Simplest tests shouldn't require an external library.
* For long term usage users of the __UTF__ should be able to build it as a standalone library.

The __UTF__ satisfies the requirements above, and provides versatile facilities to:

* Easily specify all the expectations in the code being tested.
* Organize these expectations into [link test_case test cases] and [link test_suite test suites].
* Detect different kinds of errors, failures, time-outs and report them in a uniform customizable way.

[h4:why_framework Why do you need a framework?]

While you can write a testing program yourself from scratch, the framework offers the following benefits:

* You get an error report in a text format. Error reports are uniform and you can easily machine-analyze them.
* Error reporting is separated from the testing code. You can easily change the error report format without affecting the testing code.
* The framework automatically detects exceptions thrown by the tested components and time-outs, and reports them along other errors.
* You can easily filter the test cases, and call only the desired ones. This does not require changing the testing code.

[endsect][/ design_rationale]

[section:how_to_read How to read this documentation]

This documentation is structured by what *you*, as a user, need to know to successfully use the __UTF__ and the order of decisions
you have to make and order of complexity of the problems you might encounter. If you ever find yourself facing with some unclear
term feel free to jump directly to the [link boost_test.section_glossary glossary] section, where short definitions for all used
terms were collected.

Typically, when writing a test module using the __UTF__ you have to go through the following steps:

* You decide how you want to incorporate the __UTF__: `#include` it as a header-only library, or link with it as a static library,
  or use it as a shared (or dynamically loaded) library. For details on this topic see section [link boost_test.usage_variants Usage variants].
* You add a [link test_case test case] into a [link ref_test_tree test tree].
  For details, see section [link boost_test.tests_organization.test_cases Test cases].
* You perform correctness checks of the code under tested. For details, see section [link boost_test.testing_tools Writing unit tests].
* You perform the initialization of code under test before each test case.
  For details, see section [link boost_test.tests_organization.fixtures Fixtures].
* You might want to customize the way test failures are reported. For details, see section [link boost_test.test_output Controlling output].
* You can control the run-time behavior of the built test module (e.g., run only selected tests, change the output format).
  This is covered in section [link boost_test.runtime_config Runtime configuration].

If you can't find answer to your question in any of the section mentioned above or if you believe you need even more configuration options,
you can check [link boost_test.adv_scenarios Advanced usage scenarios] section.

[endsect][/ how_to_read]

[/ EOF]
