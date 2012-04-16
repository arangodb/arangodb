#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for Windows builds.

These functions are executed via gyp-win-tool when using the ninja generator.
"""

import os
import shutil
import sys


def main(args):
  executor = WinTool()
  exit_code = executor.Dispatch(args)
  if exit_code is not None:
    sys.exit(exit_code)


class WinTool(object):
  """This class performs all the Windows tooling steps. The methods can either
  be executed directly, or dispatched from an argument list."""

  def Dispatch(self, args):
    """Dispatches a string command to a method."""
    if len(args) < 1:
      raise Exception("Not enough arguments")

    method = "Exec%s" % self._CommandifyName(args[0])
    return getattr(self, method)(*args[1:])

  def _CommandifyName(self, name_string):
    """Transforms a tool name like recursive-mirror to RecursiveMirror."""
    return name_string.title().replace('-', '')

  def ExecRecursiveMirror(self, source, dest):
    """Emulation of rm -rf out && cp -af in out."""
    if os.path.exists(dest):
      if os.path.isdir(dest):
        shutil.rmtree(dest)
      else:
        os.unlink(dest)
    if os.path.isdir(source):
      shutil.copytree(source, dest)
    else:
      shutil.copy2(source, dest)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
