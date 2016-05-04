#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Uses different APIs to touch a file."""

import os
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
  print 'Only look if a file exists but do not open it.'
  assert len(sys.argv) == 2
  path = os.path.join(BASE_DIR, 'test_file.txt')
  command = sys.argv[1]
  if command == 'access':
    return not os.access(path, os.R_OK)
  elif command == 'isfile':
    return not os.path.isfile(path)
  elif command == 'stat':
    return not os.stat(path).st_size
  return 1


if __name__ == '__main__':
  sys.exit(main())
