#!/usr/bin/env python
# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for analyzer
"""

import TestGyp

found = 'Found dependency\n'
not_found = 'No dependencies\n'

def __CreateTestFile(files):
  f = open('test_file', 'w')
  for file in files:
    f.write(file + '\n')
  f.close()

test = TestGyp.TestGypCustom(format='analyzer')

# Verifies file_path must be specified.
test.run_gyp('test.gyp',
             stdout='Must specify files to analyze via file_path generator '
             'flag\n')

# Trivial test of a source.
__CreateTestFile(['foo.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

# Conditional source that is excluded.
__CreateTestFile(['conditional_source.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=not_found)

# Conditional source that is included by way of argument.
__CreateTestFile(['conditional_source.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', '-Dtest_variable=1',
             stdout=found)

# Two unknown files.
__CreateTestFile(['unknown1.c', 'unoknow2.cc'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=not_found)

# Two unknown files.
__CreateTestFile(['unknown1.c', 'subdir/subdir_sourcex.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=not_found)

# Included dependency
__CreateTestFile(['unknown1.c', 'subdir/subdir_source.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

# Included inputs to actions.
__CreateTestFile(['action_input.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

# Don't consider outputs.
__CreateTestFile(['action_output.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=not_found)

# Rule inputs.
__CreateTestFile(['rule_input.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

# Ignore patch specified with PRODUCT_DIR.
__CreateTestFile(['product_dir_input.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=not_found)

# Path specified via a variable.
__CreateTestFile(['subdir/subdir_source2.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

# Verifies paths with // are fixed up correctly.
__CreateTestFile(['parent_source.c'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

# Verifies relative paths are resolved correctly.
__CreateTestFile(['subdir/subdir_source.h'])
test.run_gyp('test.gyp', '-Gfile_path=test_file', stdout=found)

test.pass_test()
