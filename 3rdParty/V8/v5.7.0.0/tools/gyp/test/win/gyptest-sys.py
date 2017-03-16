#!/usr/bin/env python

# Copyright (c) 2016 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that Windows drivers are built correctly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs'])

  CHDIR = 'win-driver-target-type'
  test.run_gyp('win-driver-target-type.gyp', chdir=CHDIR)
  test.build('win-driver-target-type.gyp', 'win_driver_target_type',
      chdir=CHDIR)

  test.pass_test()
