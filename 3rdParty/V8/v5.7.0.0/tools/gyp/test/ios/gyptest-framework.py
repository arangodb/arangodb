#!/usr/bin/env python

# Copyright 2016 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ios app frameworks are built correctly.
"""

import TestGyp
import TestMac
import subprocess
import sys

if sys.platform == 'darwin' and TestMac.Xcode.Version()>="0700":

  test = TestGyp.TestGyp(formats=['ninja'])
  if test.format == 'xcode-ninja':
    test.skip_test()

  test.run_gyp('framework.gyp', chdir='framework')

  test.build('framework.gyp', 'iOSFramework', chdir='framework')

  test.built_file_must_exist(
      'iOSFramework.framework/Headers/iOSFramework.h',
      chdir='framework')
  test.built_file_must_exist(
      'iOSFramework.framework/Headers/Thing.h',
      chdir='framework')
  test.built_file_must_exist(
      'iOSFramework.framework/iOSFramework',
      chdir='framework')

  test.pass_test()

