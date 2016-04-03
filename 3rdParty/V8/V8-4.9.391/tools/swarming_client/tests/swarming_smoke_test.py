#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import subprocess
import sys
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

ISOLATE_SERVER = 'https://isolateserver.appspot.com/'
SWARMING_SERVER = 'https://chromium-swarm.appspot.com/'


class TestSwarm(unittest.TestCase):
  def test_example(self):
    # pylint: disable=W0101
    # A user should be able to trigger a swarm job and return results.
    cmd = [
      sys.executable,
      '3_swarming_trigger_collect.py',
      '--isolate-server', ISOLATE_SERVER,
      '--swarming', SWARMING_SERVER,
    ]
    if '-v' in sys.argv:
      cmd.append('--verbose')
    p = subprocess.Popen(
        cmd,
        cwd=os.path.join(BASE_DIR, '..', 'example'),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    out = p.communicate()[0]
    logging.debug(out)
    self.assertEqual(0, p.returncode, out)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
