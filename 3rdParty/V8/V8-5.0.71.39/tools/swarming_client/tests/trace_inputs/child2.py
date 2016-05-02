#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import sys
import time


def main():
  print 'child2'
  # Introduce a race condition with the parent so the parent may have a chance
  # to exit before the child. Will be random.
  time.sleep(.01)

  if sys.platform in ('darwin', 'win32'):
    # Check for case-insensitive file system. This happens on Windows and OSX.
    # The log should still list test_file.txt.
    open('Test_File.txt', 'rb').close()
  else:
    open('test_file.txt', 'rb').close()

  expected = {
    'bar': 'Foo\n',
    'foo': 'Bar\n',
  }

  root = 'files1'
  actual = dict(
      (filename, open(os.path.join(root, filename), 'rb').read())
      for filename in (os.listdir(root))
      if (filename != 'do_not_care.txt' and
          os.path.isfile(os.path.join(root, filename))))

  if actual != expected:
    print 'Failure'
    print actual
    print expected
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
