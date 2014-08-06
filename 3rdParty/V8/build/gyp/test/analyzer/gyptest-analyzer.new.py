#!/usr/bin/env python
# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for analyzer
"""

import json
import TestGyp

# TODO(sky): when done migrating recipes rename to gyptest-analyzer and nuke
# existing gyptest-analyzer.

found = 'Found dependency'
not_found = 'No dependencies'

def _CreateTestFile(files, targets):
  f = open('test_file', 'w')
  to_write = {'files': files, 'targets': targets }
  json.dump(to_write, f)
  f.close()

def _CreateBogusTestFile():
  f = open('test_file','w')
  f.write('bogus')
  f.close()

def _ReadOutputFileContents():
  f = open('analyzer_output', 'r')
  result = json.load(f)
  f.close()
  return result

# NOTE: this would be clearer if it subclassed TestGypCustom, but that trips
# over a bug in pylint (E1002).
test = TestGyp.TestGypCustom(format='analyzer')

def run_analyzer(*args, **kw):
  """Runs the test specifying a particular config and output path."""
  args += ('-Gconfig_path=test_file',
           '-Ganalyzer_output_path=analyzer_output')
  test.run_gyp('test.gyp', *args, **kw)

def EnsureContains(targets=set(), matched=False):
  """Verifies output contains |targets| and |direct_targets|."""
  result = _ReadOutputFileContents()
  if result.get('error', None):
    print 'unexpected error', result.get('error')
    test.fail_test()

  if result.get('warning', None):
    print 'unexpected warning', result.get('warning')
    test.fail_test()

  actual_targets = set(result['targets'])
  if actual_targets != targets:
    print 'actual targets:', actual_targets, '\nexpected targets:', targets
    test.fail_test()

  if matched and result['status'] != found:
    print 'expected', found, 'got', result['status']
    test.fail_test()
  elif not matched and result['status'] != not_found:
    print 'expected', not_found, 'got', result['status']
    test.fail_test()

def EnsureError(expected_error_string):
  """Verifies output contains the error string."""
  result = _ReadOutputFileContents()
  if result.get('error', '').find(expected_error_string) == -1:
    print 'actual error:', result.get('error', ''), '\nexpected error:', \
        expected_error_string
    test.fail_test()

def EnsureWarning(expected_warning_string):
  """Verifies output contains the warning string."""
  result = _ReadOutputFileContents()
  if result.get('warning', '').find(expected_warning_string) == -1:
    print 'actual warning:', result.get('warning', ''), \
        '\nexpected warning:', expected_warning_string
    test.fail_test()

# Verifies file_path must be specified.
test.run_gyp('test.gyp',
             stdout='Must specify files to analyze via file_path generator '
             'flag\n')

# Verifies config_path must point to a valid file.
test.run_gyp('test.gyp', '-Gconfig_path=bogus_file',
             '-Ganalyzer_output_path=analyzer_output')
EnsureError('Unable to open file bogus_file')

# Verify get error when bad target is specified.
_CreateTestFile(['exe2.c'], ['bad_target'])
run_analyzer()
EnsureWarning('Unable to find all targets')

# Verifies config_path must point to a valid json file.
_CreateBogusTestFile()
run_analyzer()
EnsureError('Unable to parse config file test_file')

# Trivial test of a source.
_CreateTestFile(['foo.c'], [])
run_analyzer()
EnsureContains(matched=True)

# Conditional source that is excluded.
_CreateTestFile(['conditional_source.c'], [])
run_analyzer()
EnsureContains(matched=False)

# Conditional source that is included by way of argument.
_CreateTestFile(['conditional_source.c'], [])
run_analyzer('-Dtest_variable=1')
EnsureContains(matched=True)

# Two unknown files.
_CreateTestFile(['unknown1.c', 'unoknow2.cc'], [])
run_analyzer()
EnsureContains()

# Two unknown files.
_CreateTestFile(['unknown1.c', 'subdir/subdir_sourcex.c'], [])
run_analyzer()
EnsureContains()

# Included dependency
_CreateTestFile(['unknown1.c', 'subdir/subdir_source.c'], [])
run_analyzer()
EnsureContains(matched=True)

# Included inputs to actions.
_CreateTestFile(['action_input.c'], [])
run_analyzer()
EnsureContains(matched=True)

# Don't consider outputs.
_CreateTestFile(['action_output.c'], [])
run_analyzer()
EnsureContains(matched=False)

# Rule inputs.
_CreateTestFile(['rule_input.c'], [])
run_analyzer()
EnsureContains(matched=True)

# Ignore path specified with PRODUCT_DIR.
_CreateTestFile(['product_dir_input.c'], [])
run_analyzer()
EnsureContains(matched=False)

# Path specified via a variable.
_CreateTestFile(['subdir/subdir_source2.c'], [])
run_analyzer()
EnsureContains(matched=True)

# Verifies paths with // are fixed up correctly.
_CreateTestFile(['parent_source.c'], [])
run_analyzer()
EnsureContains(matched=True)

# Verifies relative paths are resolved correctly.
_CreateTestFile(['subdir/subdir_source.h'], [])
run_analyzer()
EnsureContains(matched=True)

# Various permutations when passing in targets.
_CreateTestFile(['exe2.c', 'subdir/subdir2b_source.c'], ['exe', 'exe3'])
run_analyzer()
EnsureContains(matched=True, targets={'exe3'})

_CreateTestFile(['exe2.c', 'subdir/subdir2b_source.c'], ['exe'])
run_analyzer()
EnsureContains(matched=True)

# Verifies duplicates are ignored.
_CreateTestFile(['exe2.c', 'subdir/subdir2b_source.c'], ['exe', 'exe'])
run_analyzer()
EnsureContains(matched=True)

_CreateTestFile(['exe2.c'], ['exe'])
run_analyzer()
EnsureContains(matched=True)

_CreateTestFile(['exe2.c'], [])
run_analyzer()
EnsureContains(matched=True)

_CreateTestFile(['subdir/subdir2b_source.c', 'exe2.c'], [])
run_analyzer()
EnsureContains(matched=True)

_CreateTestFile(['exe2.c'], [])
run_analyzer()
EnsureContains(matched=True)

test.pass_test()
