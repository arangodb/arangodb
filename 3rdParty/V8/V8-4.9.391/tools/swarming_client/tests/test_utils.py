# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import sys
import unittest


_UMASK = None


class EnvVars(object):
  """Context manager for environment variables.

  Passed a dict to the constructor it sets variables named with the key to the
  value.  Exiting the context causes all the variables named with the key to be
  restored to their value before entering the context.
  """
  def __init__(self, var_map):
    self.var_map = var_map
    self._backup = None

  def __enter__(self):
    self._backup = os.environ
    os.environ = os.environ.copy()
    os.environ.update(self.var_map)

  def __exit__(self, exc_type, exc_value, traceback):
    os.environ = self._backup


class SymLink(str):
  """Used as a marker to create a symlink instead of a file."""


def umask():
  """Returns current process umask without modifying it."""
  global _UMASK
  if _UMASK is None:
    _UMASK = os.umask(0777)
    os.umask(_UMASK)
  return _UMASK


def make_tree(out, contents):
  for relpath, content in sorted(contents.iteritems()):
    filepath = os.path.join(out, relpath.replace('/', os.path.sep))
    dirpath = os.path.dirname(filepath)
    if not os.path.isdir(dirpath):
      os.makedirs(dirpath, 0700)
    if isinstance(content, SymLink):
      os.symlink(content, filepath)
    else:
      mode = 0700 if relpath.endswith('.py') else 0600
      flags = os.O_WRONLY | os.O_CREAT
      if sys.platform == 'win32':
        flags |= os.O_BINARY
      with os.fdopen(os.open(filepath, flags, mode), 'wb') as f:
        f.write(content)


def main():
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  # Use an unusual umask.
  os.umask(0070)
  unittest.main()
