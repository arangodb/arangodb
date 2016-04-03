# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__version__ = '1.0'

import collections
import inspect
import unittest


class AutoStubMixIn(object):
  """Automatically restores stubbed functions on unit test teardDown.

  It's an extremely lightweight mocking class that doesn't require bookeeping.
  """
  _saved = None

  def mock(self, obj, member, mock):
    self._saved = self._saved or collections.OrderedDict()
    old_value = self._saved.setdefault(
        obj, collections.OrderedDict()).setdefault(member, getattr(obj, member))
    setattr(obj, member, mock)
    return old_value

  def tearDown(self):
    """Restore all the mocked members."""
    if self._saved:
      for obj, items in self._saved.iteritems():
        for member, previous_value in items.iteritems():
          setattr(obj, member, previous_value)


class SimpleMock(object):
  """Really simple manual class mock."""
  def __init__(self, unit_test):
    """Do not call __init__ if you want to use the global call list to detect
    ordering across different instances.
    """
    self.calls = []
    self.unit_test = unit_test
    self.assertEqual = unit_test.assertEqual

  def pop_calls(self):
    """Returns the list of calls up to date.

    Good to do self.assertEqual(expected, mock.pop_calls()).
    """
    calls = self.calls
    self.calls = []
    return calls

  def check_calls(self, expected):
    self.assertEqual(expected, self.pop_calls())

  def _register_call(self, *args, **kwargs):
    """Registers the name of the caller function."""
    caller_name = kwargs.pop('caller_name', None) or inspect.stack()[1][3]
    str_args = ', '.join(repr(arg) for arg in args)
    str_kwargs = ', '.join('%s=%r' % (k, v) for k, v in kwargs.iteritems())
    self.calls.append('%s(%s)' % (
        caller_name, ', '.join(filter(None, [str_args, str_kwargs]))))


class TestCase(unittest.TestCase, AutoStubMixIn):
  """Adds self.mock() and self.has_failed() to a TestCase."""
  def tearDown(self):
    AutoStubMixIn.tearDown(self)
    unittest.TestCase.tearDown(self)

  def has_failed(self):
    """Returns True if the test has failed."""
    return not self._resultForDoCleanups.wasSuccessful()
