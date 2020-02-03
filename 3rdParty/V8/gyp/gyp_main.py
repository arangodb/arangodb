#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import

import os
import sys
import subprocess

PY3 = bytes != str

# Below IsCygwin() function copied from pylib/gyp/common.py
def IsCygwin():
  try:
    out = subprocess.Popen("uname",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)
    stdout, stderr = out.communicate()
    if PY3:
      stdout = stdout.decode("utf-8")
    return "CYGWIN" in str(stdout)
  except Exception:
    return False


def UnixifyPath(path):
  try:
    if not IsCygwin():
      return path
    out = subprocess.Popen(["cygpath", "-u", path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)
    stdout, stderr = out.communicate()
    if PY3:
      stdout = stdout.decode("utf-8")
    return str(stdout)
  except Exception:
    return path


# Make sure we're using the version of pylib in this repo, not one installed
# elsewhere on the system. Also convert to Unix style path on Cygwin systems,
# else the 'gyp' library will not be found
path = UnixifyPath(sys.argv[0])
p = os.path.dirname(path).replace('/cygdrive/c/', 'c:/')
sys.path.insert(0, p)
sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))
sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../gypfiles'))
sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../v7.9.317/third_party/binutils'))
#print(sys.path)
sys.argv.append("-DPYTHON_EXECUTABLE=" + sys.executable)
os.environ['PYTHON_EXECUTABLE'] = sys.executable
import gyp

if __name__ == '__main__':
  sys.exit(gyp.script_main())
