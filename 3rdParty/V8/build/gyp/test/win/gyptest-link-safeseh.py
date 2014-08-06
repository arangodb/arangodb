#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure safeseh setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('safeseh.gyp', chdir=CHDIR)
  test.build('safeseh.gyp', test.ALL, chdir=CHDIR)

  def HasSafeExceptionHandlers(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    output = test.run_dumpbin('/LOADCONFIG', full_path)
    return '    Safe Exception Handler Table' in output

  # From MSDN: http://msdn.microsoft.com/en-us/library/9a89h429.aspx
  #   If /SAFESEH is not specified, the linker will produce an image with a
  #   table of safe exceptions handlers if all modules are compatible with
  #   the safe exception handling feature. If any modules were not
  #   compatible with safe exception handling feature, the resulting image
  #   will not contain a table of safe exception handlers.
  if HasSafeExceptionHandlers('test_safeseh_default.exe'):
    test.fail_test()
  if HasSafeExceptionHandlers('test_safeseh_no.exe'):
    test.fail_test()
  if not HasSafeExceptionHandlers('test_safeseh_yes.exe'):
    test.fail_test()

  test.pass_test()
