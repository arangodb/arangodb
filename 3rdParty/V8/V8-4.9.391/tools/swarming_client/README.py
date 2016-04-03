#!/usr/bin/env python
# coding=utf-8
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

###
# Run me to generate the documentation!
###

# Line too long (NN/80)
# pylint: disable=C0301

"""Test tracing and isolation infrastructure.

A few scripts have strict dependency rules:
- The pure tracing scripts (trace_*.py) do not know about isolate
  infrastructure.
"""

import os
import sys


def main():
  for i in sorted(os.listdir(os.path.dirname(os.path.abspath(__file__)))):
    if not i.endswith('.py') or i == 'PRESUBMIT.py':
      continue
    module = __import__(i[:-3])
    if hasattr(module, '__doc__'):
      print module.__name__
      print ''.join('  %s\n' % i for i in module.__doc__.splitlines())
  return 0


if __name__ == '__main__':
  sys.exit(main())
