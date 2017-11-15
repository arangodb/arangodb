#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure msvs_application_type_revision works correctly.
"""

import TestGyp

import os
import sys
import struct

CHDIR = 'winrt-app-type-revision'

print 'This test is not currently working on the bots: https://code.google.com/p/gyp/issues/detail?id=466'
sys.exit(0)

if (sys.platform == 'win32' and
    int(os.environ.get('GYP_MSVS_VERSION', 0)) == 2013):
  test = TestGyp.TestGyp(formats=['msvs'])

  test.run_gyp('winrt-app-type-revision.gyp', chdir=CHDIR)

  test.build('winrt-app-type-revision.gyp', 'enable_winrt_81_revision_dll',
             chdir=CHDIR)

  # Revision is set to 8.2 which is invalid for 2013 projects so compilation
  # must fail.
  test.build('winrt-app-type-revision.gyp', 'enable_winrt_82_revision_dll',
             chdir=CHDIR, status=1)

  # Revision is set to an invalid value for 2013 projects so compilation
  # must fail.
  test.build('winrt-app-type-revision.gyp', 'enable_winrt_invalid_revision_dll',
             chdir=CHDIR, status=1)

  test.pass_test()
