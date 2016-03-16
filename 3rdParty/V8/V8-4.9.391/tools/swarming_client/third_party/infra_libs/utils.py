# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Miscellaneous utility functions."""


import contextlib
import errno
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time


def read_json_as_utf8(filename=None, text=None):
  """Read and deserialize a json file or string.

  This function is different from json.load and json.loads in that it
  returns utf8-encoded string for keys and values instead of unicode.

  Args:
    filename (str): path of a file to parse
    text (str): json string to parse

  ``filename`` and ``text`` are mutually exclusive. ValueError is raised if
  both are provided.
  """

  if filename is not None and text is not None:
    raise ValueError('Only one of "filename" and "text" can be provided at '
                     'the same time')

  if filename is None and text is None:
    raise ValueError('One of "filename" and "text" must be provided')

  def to_utf8(obj):
    if isinstance(obj, dict):
      return {to_utf8(key): to_utf8(value) for key, value in obj.iteritems()}
    if isinstance(obj, list):
      return [to_utf8(item) for item in obj]
    if isinstance(obj, unicode):
      return obj.encode('utf-8')
    return obj

  if filename:
    with open(filename, 'rb') as f:
      obj = json.load(f)
  else:
    obj = json.loads(text)

  return to_utf8(obj)


# TODO(hinoka): Add tests crbug.com/500781
def rmtree(file_path):  # pragma: no cover
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at file_path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  if not os.path.exists(file_path):
    return

  if sys.platform == 'win32':
    # Give up and use cmd.exe's rd command.
    file_path = os.path.normcase(file_path)
    for _ in xrange(3):
      if not subprocess.call(['cmd.exe', '/c', 'rd', '/q', '/s', file_path]):
        break
      time.sleep(3)
    return

  def remove_with_retry(rmfunc, path):
    if os.path.islink(path):
      return os.remove(path)
    else:
      return rmfunc(path)

  def rmtree_on_error(function, _, excinfo):
    """This works around a problem whereby python 2.x on Windows has no ability
    to check for symbolic links.  os.path.islink always returns False.  But
    shutil.rmtree will fail if invoked on a symbolic link whose target was
    deleted before the link.  E.g., reproduce like this:
    > mkdir test
    > mkdir test\1
    > mklink /D test\current test\1
    > python -c "import infra_libs; infra_libs.rmtree('test')"
    To avoid this issue, we pass this error-handling function to rmtree.  If
    we see the exact sort of failure, we ignore it.  All other failures we re-
    raise.
    """

    exception_type = excinfo[0]
    exception_value = excinfo[1]
    # If shutil.rmtree encounters a symbolic link on Windows, os.listdir will
    # fail with a WindowsError exception with an ENOENT errno (i.e., file not
    # found).  We'll ignore that error.  Note that WindowsError is not defined
    # for non-Windows platforms, so we use OSError (of which it is a subclass)
    # to avoid lint complaints about an undefined global on non-Windows
    # platforms.
    if (function is os.listdir) and issubclass(exception_type, OSError):
      if exception_value.errno != errno.ENOENT:
        raise
    else:
      raise

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(lambda p: shutil.rmtree(p, onerror=rmtree_on_error),
                        os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)


# We're trying to be compatible with Python3 tempfile.TemporaryDirectory
# context manager here. And they used 'dir' as a keyword argument.
# pylint: disable=redefined-builtin
@contextlib.contextmanager
def temporary_directory(suffix="", prefix="tmp", dir=None,
                        keep_directory=False):
  """Create and return a temporary directory.  This has the same
  behavior as mkdtemp but can be used as a context manager.  For
  example:

    with temporary_directory() as tmpdir:
      ...

  Upon exiting the context, the directory and everything contained
  in it are removed.

  Args:
    suffix, prefix, dir: same arguments as for tempfile.mkdtemp.
    keep_directory (bool): if True, do not delete the temporary directory
      when exiting. Useful for debugging.

  Returns:
    tempdir (str): full path to the temporary directory.
  """
  tempdir = None  # Handle mkdtemp raising an exception
  try:
    tempdir = tempfile.mkdtemp(suffix, prefix, dir)
    yield tempdir

  finally:
    if tempdir and not keep_directory:  # pragma: no branch
      try:
        # TODO(pgervais,496347) Make this work reliably on Windows.
        shutil.rmtree(tempdir, ignore_errors=True)
      except OSError as ex:  # pragma: no cover
        print >> sys.stderr, (
          "ERROR: {!r} while cleaning up {!r}".format(ex, tempdir))
