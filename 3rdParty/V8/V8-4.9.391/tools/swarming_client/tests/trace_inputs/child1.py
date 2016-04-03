#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import subprocess
import sys


def child():
  """When the gyp argument is not specified, the command is started from
  --root-dir directory.
  """
  print 'child from %s' % os.getcwd()
  # Force file opening with a non-normalized path.
  open(os.path.join('tests', '..', 'trace_inputs.py'), 'rb').close()
  open(os.path.join(
      'tests', '..', 'tests', 'trace_inputs_smoke_test.py'), 'rb').close()
  # Do not wait for the child to exit.
  # Use relative directory.
  subprocess.Popen(
      ['python', 'child2.py'], cwd=os.path.join('tests', 'trace_inputs'))
  return 0


def child_gyp():
  """When the gyp argument is specified, the command is started from --cwd
  directory.
  """
  print 'child_gyp from %s' % os.getcwd()
  # Force file opening.
  open(os.path.join('..', 'trace_inputs.py'), 'rb').close()
  open(os.path.join('..', 'tests', 'trace_inputs_smoke_test.py'), 'rb').close()
  # Do not wait for the child to exit.
  # Use relative directory.
  subprocess.Popen(['python', 'child2.py'], cwd='trace_inputs')
  return 0


def main():
  if sys.argv[1] == '--child':
    return child()
  if sys.argv[1] == '--child-gyp':
    return child_gyp()
  return 1


if __name__ == '__main__':
  sys.exit(main())
