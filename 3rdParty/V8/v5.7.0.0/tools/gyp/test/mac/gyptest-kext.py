#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that kext bundles are built correctly.
"""

import TestGyp
import TestMac

import os
import plistlib
import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])
  test.run_gyp('kext.gyp', chdir='kext')
  test.build('kext.gyp', test.ALL, chdir='kext')
  test.built_file_must_exist('GypKext.kext/Contents/MacOS/GypKext',
                             chdir='kext')
  test.built_file_must_exist('GypKext.kext/Contents/Info.plist',
                             chdir='kext')
  test.pass_test()
