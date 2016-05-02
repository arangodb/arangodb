#!/usr/bin/env python
# Copyright 2015 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import subprocess
import sys
import tempfile
import unittest
import re

THIS_FILE = os.path.abspath(__file__)
sys.path.insert(0, os.path.dirname(os.path.dirname(THIS_FILE)))

from third_party.depot_tools import fix_encoding
from utils import file_path
from utils import logging_utils


# PID YYYY-MM-DD HH:MM:SS.MMM
_LOG_HEADER = r'^%d \d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d' % os.getpid()
_LOG_HEADER_PID = r'^\d+ \d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d'


_PHASE = 'LOGGING_UTILS_TESTS_PHASE'


def call(phase, cwd):
  """Calls itself back."""
  env = os.environ.copy()
  env[_PHASE] = phase
  return subprocess.call([sys.executable, '-u', THIS_FILE], env=env, cwd=cwd)


class Test(unittest.TestCase):
  def setUp(self):
    super(Test, self).setUp()
    self.tmp = tempfile.mkdtemp(prefix='logging_utils')

  def tearDown(self):
    try:
      file_path.rmtree(self.tmp)
    finally:
      super(Test, self).tearDown()

  def test_capture(self):
    root = logging.RootLogger(logging.DEBUG)
    with logging_utils.CaptureLogs('foo', root) as log:
      root.debug('foo')
      result = log.read()
    expected = _LOG_HEADER + ': DEBUG foo\n$'
    if sys.platform == 'win32':
      expected = expected.replace('\n', '\r\n')
    self.assertTrue(re.match(expected, result), (expected, result))

  def test_prepare_logging(self):
    root = logging.RootLogger(logging.DEBUG)
    filepath = os.path.join(self.tmp, 'test.log')
    logging_utils.prepare_logging(filepath, root)
    root.debug('foo')
    with open(filepath, 'rb') as f:
      result = f.read()
    # It'd be nice to figure out a way to ensure it's properly in UTC but it's
    # tricky to do reliably.
    expected = _LOG_HEADER + ' D: foo\n$'
    self.assertTrue(re.match(expected, result), (expected, result))

  def test_rotating(self):
    # Create a rotating log. Create a subprocess then delete the file. Make sure
    # nothing blows up.
    # Everything is done in a child process because the called functions mutate
    # the global state.
    self.assertEqual(0, call('test_rotating_phase_1', cwd=self.tmp))
    self.assertEqual({'shared.1.log'}, set(os.listdir(self.tmp)))
    with open(os.path.join(self.tmp, 'shared.1.log'), 'rb') as f:
      lines = f.read().splitlines()
    expected = [
      r' I: Parent1',
      r' I: Child1',
      r' I: Child2',
      r' I: Parent2',
    ]
    for e, l in zip(expected, lines):
      ex = _LOG_HEADER_PID + e + '$'
      self.assertTrue(re.match(ex, l), (ex, l))
    self.assertEqual(len(expected), len(lines))


def test_rotating_phase_1():
  logging_utils.prepare_logging('shared.log')
  logging.info('Parent1')
  r = call('test_rotating_phase_2', None)
  logging.info('Parent2')
  return r


def test_rotating_phase_2():
  # Simulate rotating the log.
  logging_utils.prepare_logging('shared.log')
  logging.info('Child1')
  os.rename('shared.log', 'shared.1.log')
  logging.info('Child2')
  return 0


def main():
  phase = os.environ.get(_PHASE)
  if phase:
    return getattr(sys.modules[__name__], phase)()
  verbose = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if verbose else logging.ERROR)
  unittest.main()


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  sys.exit(main())
