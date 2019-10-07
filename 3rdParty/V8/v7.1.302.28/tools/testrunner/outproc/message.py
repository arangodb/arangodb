# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import os
import re

from . import base


class OutProc(base.OutProc):
  def __init__(self, expected_outcomes, basepath, expected_fail):
    super(OutProc, self).__init__(expected_outcomes)
    self._basepath = basepath
    self._expected_fail = expected_fail

  def _is_failure_output(self, output):
    fail = output.exit_code != 0
    if fail != self._expected_fail:
      return True

    expected_lines = []
    # Can't use utils.ReadLinesFrom() here because it strips whitespace.
    with open(self._basepath + '.out') as f:
      for line in f:
        if line.startswith("#") or not line.strip():
          continue
        expected_lines.append(line)
    raw_lines = output.stdout.splitlines()
    actual_lines = [ s for s in raw_lines if not self._ignore_line(s) ]
    if len(expected_lines) != len(actual_lines):
      return True

    env = {
      'basename': os.path.basename(self._basepath + '.js'),
    }
    for (expected, actual) in itertools.izip_longest(
        expected_lines, actual_lines, fillvalue=''):
      pattern = re.escape(expected.rstrip() % env)
      pattern = pattern.replace('\\*', '.*')
      pattern = pattern.replace('\\{NUMBER\\}', '\d+(?:\.\d*)?')
      pattern = '^%s$' % pattern
      if not re.match(pattern, actual):
        return True
    return False

  def _ignore_line(self, string):
    """Ignore empty lines, valgrind output, Android output."""
    return (
      not string or
      not string.strip() or
      string.startswith("==") or
      string.startswith("**") or
      string.startswith("ANDROID")
    )
