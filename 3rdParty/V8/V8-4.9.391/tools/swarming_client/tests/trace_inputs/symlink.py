#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import sys


def main():
  print 'symlink: touches files2/'
  assert len(sys.argv) == 1

  expected = {
    'bar': 'Foo\n',
    'foo': 'Bar\n',
  }

  if not os.path.basename(os.getcwd()) == 'tests':
    print 'Start this script from inside "tests"'
    return 1

  root = os.path.join('trace_inputs', 'files2')
  actual = dict(
      (filename, open(os.path.join(root, filename), 'rb').read())
      for filename in (os.listdir(root)) if filename != '.svn')

  if actual != expected:
    print 'Failure'
    print actual
    print expected
    return 2
  return 0


if __name__ == '__main__':
  sys.exit(main())
