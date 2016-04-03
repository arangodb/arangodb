#!/usr/bin/env python
# coding=utf-8
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import sys


def main():
  """Creates a file name with whitespaces and comma in it."""
  # We could test ?><:*|"' and chr(1 to 32) on linux.
  # We could test ?<>*|"' on OSX.
  # On Windows, skip the Chinese characters for now as the log parsing code is
  # using the current code page to generate the log.
  if sys.platform == 'win32':
    filename = u'foo, bar,  ~p#o,,ué^t%t .txt'
  else:
    filename = u'foo, bar,  ~p#o,,ué^t%t 和平.txt'
  with open(filename, 'w') as f:
    f.write('Bingo!')
  return 0


if __name__ == '__main__':
  sys.exit(main())
