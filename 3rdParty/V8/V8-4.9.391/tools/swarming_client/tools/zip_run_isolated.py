#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Converts run_isolated.py with dependencies into run_isolated.zip.

run_isolated.zip will be created in the current directory.

Useful for reproducing swarm-bot environment like this:
  ./tools/zip_run_isolated.py && python run_isolated.zip ...
"""

import os
import sys

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_isolated


def main():
  zip_package = run_isolated.get_as_zip_package()
  zip_package.zip_into_file('run_isolated.zip')
  return 0


if __name__ == '__main__':
  sys.exit(main())
