#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""This script is meant to be run on a Swarming slave."""

import os
import sys


def main():
  print('Hello world: ' + sys.argv[1])
  if len(sys.argv) == 3:
    # Write a file in ${ISOLATED_OUTDIR}.
    with open(os.path.join(sys.argv[2], 'happiness.txt'), 'wb') as f:
      f.write(
          'is where you look %d/%d' % (
            int(os.environ['GTEST_SHARD_INDEX']),
            int(os.environ['GTEST_TOTAL_SHARDS'])))
  return 0


if __name__ == '__main__':
  sys.exit(main())
