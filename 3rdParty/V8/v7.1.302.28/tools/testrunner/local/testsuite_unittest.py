#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.local.testsuite import TestSuite
from testrunner.objects.testcase import TestCase


class TestSuiteTest(unittest.TestCase):
  def test_filter_testcases_by_status_first_pass(self):
    suite = TestSuite('foo', 'bar')
    suite.rules = {
      '': {
        'foo/bar': set(['PASS', 'SKIP']),
        'baz/bar': set(['PASS', 'FAIL']),
      },
    }
    suite.prefix_rules = {
      '': {
        'baz/': set(['PASS', 'SLOW']),
      },
    }
    suite.tests = [
      TestCase(suite, 'foo/bar', 'foo/bar'),
      TestCase(suite, 'baz/bar', 'baz/bar'),
    ]
    suite.FilterTestCasesByStatus()
    self.assertEquals(
        [TestCase(suite, 'baz/bar', 'baz/bar')],
        suite.tests,
    )
    outcomes = suite.GetStatusFileOutcomes(suite.tests[0].name,
                                           suite.tests[0].variant)
    self.assertEquals(set(['PASS', 'FAIL', 'SLOW']), outcomes)

  def test_filter_testcases_by_status_second_pass(self):
    suite = TestSuite('foo', 'bar')

    suite.rules = {
      '': {
        'foo/bar': set(['PREV']),
      },
      'default': {
        'foo/bar': set(['PASS', 'SKIP']),
        'baz/bar': set(['PASS', 'FAIL']),
      },
      'stress': {
        'baz/bar': set(['SKIP']),
      },
    }
    suite.prefix_rules = {
      '': {
        'baz/': set(['PREV']),
      },
      'default': {
        'baz/': set(['PASS', 'SLOW']),
      },
      'stress': {
        'foo/': set(['PASS', 'SLOW']),
      },
    }

    test1 = TestCase(suite, 'foo/bar', 'foo/bar')
    test2 = TestCase(suite, 'baz/bar', 'baz/bar')
    suite.tests = [
      test1.create_variant(variant='default', flags=[]),
      test1.create_variant(variant='stress', flags=['-v']),
      test2.create_variant(variant='default', flags=[]),
      test2.create_variant(variant='stress', flags=['-v']),
    ]

    suite.FilterTestCasesByStatus()
    self.assertEquals(
        [
          TestCase(suite, 'foo/bar', 'foo/bar').create_variant(None, ['-v']),
          TestCase(suite, 'baz/bar', 'baz/bar'),
        ],
        suite.tests,
    )

    self.assertEquals(
        set(['PREV', 'PASS', 'SLOW']),
        suite.GetStatusFileOutcomes(suite.tests[0].name,
                                    suite.tests[0].variant),
    )
    self.assertEquals(
        set(['PREV', 'PASS', 'FAIL', 'SLOW']),
        suite.GetStatusFileOutcomes(suite.tests[1].name,
                                    suite.tests[1].variant),
    )

  def test_fail_ok_outcome(self):
    suite = TestSuite('foo', 'bar')
    suite.rules = {
      '': {
        'foo/bar': set(['FAIL_OK']),
        'baz/bar': set(['FAIL']),
      },
    }
    suite.prefix_rules = {}
    suite.tests = [
      TestCase(suite, 'foo/bar', 'foo/bar'),
      TestCase(suite, 'baz/bar', 'baz/bar'),
    ]

    for t in suite.tests:
      self.assertEquals(['FAIL'], t.expected_outcomes)


if __name__ == '__main__':
    unittest.main()
