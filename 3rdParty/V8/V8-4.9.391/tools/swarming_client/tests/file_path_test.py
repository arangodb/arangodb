#!/usr/bin/env python
# coding=utf-8
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import getpass
import logging
import os
import tempfile
import unittest
import StringIO
import subprocess
import sys
import time

BASE_DIR = unicode(os.path.dirname(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

FILE_PATH = unicode(os.path.abspath(__file__))

from depot_tools import auto_stub
from depot_tools import fix_encoding
import test_utils
from utils import file_path
from utils import fs


def write_content(filepath, content):
  with fs.open(filepath, 'wb') as f:
    f.write(content)


class FilePathTest(auto_stub.TestCase):
  def setUp(self):
    super(FilePathTest, self).setUp()
    self._tempdir = None

  def tearDown(self):
    try:
      if self._tempdir:
        for dirpath, dirnames, filenames in fs.walk(
            self._tempdir, topdown=True):
          for filename in filenames:
            file_path.set_read_only(os.path.join(dirpath, filename), False)
          for dirname in dirnames:
            file_path.set_read_only(os.path.join(dirpath, dirname), False)
        file_path.rmtree(self._tempdir)
    finally:
      super(FilePathTest, self).tearDown()

  @property
  def tempdir(self):
    if not self._tempdir:
      self._tempdir = tempfile.mkdtemp(prefix=u'run_isolated_test')
    return self._tempdir

  def assertFileMode(self, filepath, mode, umask=None):
    umask = test_utils.umask() if umask is None else umask
    actual = fs.stat(filepath).st_mode
    expected = mode & ~umask
    self.assertEqual(
        expected,
        actual,
        (filepath, oct(expected), oct(actual), oct(umask)))

  def assertMaskedFileMode(self, filepath, mode):
    """It's usually when the file was first marked read only."""
    self.assertFileMode(filepath, mode, 0 if sys.platform == 'win32' else 077)

  def test_native_case_end_with_os_path_sep(self):
    # Make sure the trailing os.path.sep is kept.
    path = file_path.get_native_path_case(ROOT_DIR) + os.path.sep
    self.assertEqual(file_path.get_native_path_case(path), path)

  def test_native_case_end_with_dot_os_path_sep(self):
    path = file_path.get_native_path_case(ROOT_DIR + os.path.sep)
    self.assertEqual(
        file_path.get_native_path_case(path + '.' + os.path.sep),
        path)

  def test_native_case_non_existing(self):
    # Make sure it doesn't throw on non-existing files.
    non_existing = 'trace_input_test_this_file_should_not_exist'
    path = os.path.expanduser('~/' + non_existing)
    self.assertFalse(os.path.exists(path))
    path = file_path.get_native_path_case(ROOT_DIR) + os.path.sep
    self.assertEqual(file_path.get_native_path_case(path), path)

  def test_delete_wd_rf(self):
    # Confirms that a RO file in a RW directory can be deleted on non-Windows.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    fs.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    file_path.set_read_only(dir_foo, False)
    file_path.set_read_only(file_bar, True)
    self.assertFileMode(dir_foo, 040777)
    self.assertMaskedFileMode(file_bar, 0100444)
    if sys.platform == 'win32':
      # On Windows, a read-only file can't be deleted.
      with self.assertRaises(OSError):
        fs.remove(file_bar)
    else:
      fs.remove(file_bar)

  def test_delete_rd_wf(self):
    # Confirms that a Rw file in a RO directory can be deleted on Windows only.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    fs.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    file_path.set_read_only(dir_foo, True)
    file_path.set_read_only(file_bar, False)
    self.assertMaskedFileMode(dir_foo, 040555)
    self.assertFileMode(file_bar, 0100666)
    if sys.platform == 'win32':
      # A read-only directory has a convoluted meaning on Windows, it means that
      # the directory is "personalized". This is used as a signal by Windows
      # Explorer to tell it to look into the directory for desktop.ini.
      # See http://support.microsoft.com/kb/326549 for more details.
      # As such, it is important to not try to set the read-only bit on
      # directories on Windows since it has no effect other than trigger
      # Windows Explorer to look for desktop.ini, which is unnecessary.
      fs.remove(file_bar)
    else:
      with self.assertRaises(OSError):
        fs.remove(file_bar)

  def test_delete_rd_rf(self):
    # Confirms that a RO file in a RO directory can't be deleted.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    fs.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    file_path.set_read_only(dir_foo, True)
    file_path.set_read_only(file_bar, True)
    self.assertMaskedFileMode(dir_foo, 040555)
    self.assertMaskedFileMode(file_bar, 0100444)
    with self.assertRaises(OSError):
      # It fails for different reason depending on the OS. See the test cases
      # above.
      fs.remove(file_bar)

  def test_hard_link_mode(self):
    # Creates a hard link, see if the file mode changed on the node or the
    # directory entry.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    file_link = os.path.join(dir_foo, 'link')
    fs.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    file_path.hardlink(file_bar, file_link)
    self.assertFileMode(file_bar, 0100666)
    self.assertFileMode(file_link, 0100666)
    file_path.set_read_only(file_bar, True)
    self.assertMaskedFileMode(file_bar, 0100444)
    self.assertMaskedFileMode(file_link, 0100444)
    # This is bad news for Windows; on Windows, the file must be writeable to be
    # deleted, but the file node is modified. This means that every hard links
    # must be reset to be read-only after deleting one of the hard link
    # directory entry.

  def test_rmtree_unicode(self):
    subdir = os.path.join(self.tempdir, 'hi')
    fs.mkdir(subdir)
    filepath = os.path.join(
        subdir, u'\u0627\u0644\u0635\u064A\u0646\u064A\u0629')
    with fs.open(filepath, 'wb') as f:
      f.write('hi')
    # In particular, it fails when the input argument is a str.
    file_path.rmtree(str(subdir))

  if sys.platform == 'darwin':
    def test_native_case_symlink_wrong_case(self):
      base_dir = file_path.get_native_path_case(BASE_DIR)
      trace_inputs_dir = os.path.join(base_dir, 'trace_inputs')
      actual = file_path.get_native_path_case(trace_inputs_dir)
      self.assertEqual(trace_inputs_dir, actual)

      # Make sure the symlink is not resolved.
      data = os.path.join(trace_inputs_dir, 'Files2')
      actual = file_path.get_native_path_case(data)
      self.assertEqual(
          os.path.join(trace_inputs_dir, 'files2'), actual)

      data = os.path.join(trace_inputs_dir, 'Files2', '')
      actual = file_path.get_native_path_case(data)
      self.assertEqual(
          os.path.join(trace_inputs_dir, 'files2', ''), actual)

      data = os.path.join(trace_inputs_dir, 'Files2', 'Child1.py')
      actual = file_path.get_native_path_case(data)
      # TODO(maruel): Should be child1.py.
      self.assertEqual(
          os.path.join(trace_inputs_dir, 'files2', 'Child1.py'), actual)

  if sys.platform in ('darwin', 'win32'):
    def test_native_case_not_sensitive(self):
      # The home directory is almost guaranteed to have mixed upper/lower case
      # letters on both Windows and OSX.
      # This test also ensures that the output is independent on the input
      # string case.
      path = os.path.expanduser(u'~')
      self.assertTrue(os.path.isdir(path))
      path = path.replace('/', os.path.sep)
      if sys.platform == 'win32':
        # Make sure the drive letter is upper case for consistency.
        path = path[0].upper() + path[1:]
      # This test assumes the variable is in the native path case on disk, this
      # should be the case. Verify this assumption:
      self.assertEqual(path, file_path.get_native_path_case(path))
      self.assertEqual(
          file_path.get_native_path_case(path.lower()),
          file_path.get_native_path_case(path.upper()))

    def test_native_case_not_sensitive_non_existent(self):
      # This test also ensures that the output is independent on the input
      # string case.
      non_existing = os.path.join(
          'trace_input_test_this_dir_should_not_exist', 'really not', '')
      path = os.path.expanduser(os.path.join(u'~', non_existing))
      path = path.replace('/', os.path.sep)
      self.assertFalse(fs.exists(path))
      lower = file_path.get_native_path_case(path.lower())
      upper = file_path.get_native_path_case(path.upper())
      # Make sure non-existing element is not modified:
      self.assertTrue(lower.endswith(non_existing.lower()))
      self.assertTrue(upper.endswith(non_existing.upper()))
      self.assertEqual(lower[:-len(non_existing)], upper[:-len(non_existing)])

  if sys.platform == 'win32':
    def test_native_case_alternate_datastream(self):
      # Create the file manually, since tempfile doesn't support ADS.
      tempdir = unicode(tempfile.mkdtemp(prefix=u'trace_inputs'))
      try:
        tempdir = file_path.get_native_path_case(tempdir)
        basename = 'foo.txt'
        filename = basename + ':Zone.Identifier'
        filepath = os.path.join(tempdir, filename)
        open(filepath, 'w').close()
        self.assertEqual(filepath, file_path.get_native_path_case(filepath))
        data_suffix = ':$DATA'
        self.assertEqual(
            filepath + data_suffix,
            file_path.get_native_path_case(filepath + data_suffix))

        open(filepath + '$DATA', 'w').close()
        self.assertEqual(
            filepath + data_suffix,
            file_path.get_native_path_case(filepath + data_suffix))
        # Ensure the ADS weren't created as separate file. You love NTFS, don't
        # you?
        self.assertEqual([basename], fs.listdir(tempdir))
      finally:
        file_path.rmtree(tempdir)

    def test_rmtree_win(self):
      # Mock our sleep for faster test case execution.
      sleeps = []
      self.mock(time, 'sleep', sleeps.append)
      self.mock(sys, 'stderr', StringIO.StringIO())

      # Open a child process, so the file is locked.
      subdir = os.path.join(self.tempdir, 'to_be_deleted')
      fs.mkdir(subdir)
      script = 'import time; open(\'a\', \'w\'); time.sleep(60)'
      proc = subprocess.Popen([sys.executable, '-c', script], cwd=subdir)
      try:
        # Wait until the file exist.
        while not fs.isfile(os.path.join(subdir, 'a')):
          self.assertEqual(None, proc.poll())
        file_path.rmtree(subdir)
        self.assertEqual([2, 4, 2], sleeps)
        # sys.stderr.getvalue() would return a fair amount of output but it is
        # not completely deterministic so we're not testing it here.
      finally:
        proc.wait()

    def test_filter_processes_dir_win(self):
      python_dir = os.path.dirname(sys.executable)
      processes = file_path.filter_processes_dir_win(
          file_path.enum_processes_win(), python_dir)
      self.assertTrue(processes)
      proc_names = [proc.ExecutablePath for proc in processes]
      # Try to find at least one python process.
      self.assertTrue(
          any(proc == sys.executable for proc in proc_names), proc_names)

    def test_filter_processes_tree_win(self):
      # Create a grand-child.
      script = (
        'import subprocess,sys;'
        'proc = subprocess.Popen('
          '[sys.executable, \'-u\', \'-c\', \'import time; print(1); '
          'time.sleep(60)\'], stdout=subprocess.PIPE); '
        # Signal grand child is ready.
        'print(proc.stdout.read(1)); '
        # Wait for parent to have completed the test.
        'sys.stdin.read(1); '
        'proc.kill()'
      )
      proc = subprocess.Popen(
          [sys.executable, '-u', '-c', script],
          stdin=subprocess.PIPE,
          stdout=subprocess.PIPE)
      try:
        proc.stdout.read(1)
        processes = file_path.filter_processes_tree_win(
            file_path.enum_processes_win())
        self.assertEqual(3, len(processes), processes)
        proc.stdin.write('a')
        proc.wait()
      except Exception:
        proc.kill()
      finally:
        proc.wait()

  if sys.platform != 'win32':
    def test_symlink(self):
      # This test will fail if the checkout is in a symlink.
      actual = file_path.split_at_symlink(None, ROOT_DIR)
      expected = (ROOT_DIR, None, None)
      self.assertEqual(expected, actual)

      actual = file_path.split_at_symlink(
          None, os.path.join(BASE_DIR, 'trace_inputs'))
      expected = (
          os.path.join(BASE_DIR, 'trace_inputs'), None, None)
      self.assertEqual(expected, actual)

      actual = file_path.split_at_symlink(
          None, os.path.join(BASE_DIR, 'trace_inputs', 'files2'))
      expected = (
          os.path.join(BASE_DIR, 'trace_inputs'), 'files2', '')
      self.assertEqual(expected, actual)

      actual = file_path.split_at_symlink(
          ROOT_DIR, os.path.join('tests', 'trace_inputs', 'files2'))
      expected = (
          os.path.join('tests', 'trace_inputs'), 'files2', '')
      self.assertEqual(expected, actual)
      actual = file_path.split_at_symlink(
          ROOT_DIR, os.path.join('tests', 'trace_inputs', 'files2', 'bar'))
      expected = (
          os.path.join('tests', 'trace_inputs'), 'files2', '/bar')
      self.assertEqual(expected, actual)

    def test_native_case_symlink_right_case(self):
      actual = file_path.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs'))
      self.assertEqual('trace_inputs', os.path.basename(actual))

      # Make sure the symlink is not resolved.
      actual = file_path.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs', 'files2'))
      self.assertEqual('files2', os.path.basename(actual))

  else:
    def test_undeleteable_chmod(self):
      # Create a file and a directory with an empty ACL. Then try to delete it.
      dirpath = os.path.join(self.tempdir, 'd')
      filepath = os.path.join(dirpath, 'f')
      os.mkdir(dirpath)
      with open(filepath, 'w') as f:
        f.write('hi')
      os.chmod(filepath, 0)
      os.chmod(dirpath, 0)
      file_path.rmtree(dirpath)

    def test_undeleteable_owner(self):
      # Create a file and a directory with an empty ACL. Then try to delete it.
      dirpath = os.path.join(self.tempdir, 'd')
      filepath = os.path.join(dirpath, 'f')
      os.mkdir(dirpath)
      with open(filepath, 'w') as f:
        f.write('hi')
      import win32security
      user, _domain, _type = win32security.LookupAccountName(
          '', getpass.getuser())
      sd = win32security.SECURITY_DESCRIPTOR()
      sd.Initialize()
      sd.SetSecurityDescriptorOwner(user, False)
      # Create an empty DACL, which removes all rights.
      dacl = win32security.ACL()
      dacl.Initialize()
      sd.SetSecurityDescriptorDacl(1, dacl, 0)
      win32security.SetFileSecurity(
          fs.extend(filepath), win32security.DACL_SECURITY_INFORMATION, sd)
      win32security.SetFileSecurity(
          fs.extend(dirpath), win32security.DACL_SECURITY_INFORMATION, sd)
      file_path.rmtree(dirpath)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
