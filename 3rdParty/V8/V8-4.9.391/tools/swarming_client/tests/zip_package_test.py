#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import cStringIO as StringIO
import logging
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from third_party.depot_tools import fix_encoding
from utils import file_path
from utils import zip_package


def check_output(*args, **kwargs):
  return  subprocess.check_output(*args, stderr=subprocess.STDOUT, **kwargs)


class ZipPackageTest(unittest.TestCase):
  def setUp(self):
    super(ZipPackageTest, self).setUp()
    self.temp_dir = tempfile.mkdtemp(prefix=u'zip_package_test')

  def tearDown(self):
    try:
      file_path.rmtree(self.temp_dir)
    finally:
      super(ZipPackageTest, self).tearDown()

  def stage_files(self, files):
    """Populates temp directory with given files specified as a list or dict."""
    if not isinstance(files, dict):
      files = dict((p, '') for p in files)
    for path, content in files.iteritems():
      abs_path = os.path.join(self.temp_dir, path.replace('/', os.sep))
      dir_path = os.path.dirname(abs_path)
      if not os.path.exists(dir_path):
        os.makedirs(dir_path)
      with open(abs_path, 'wb') as f:
        f.write(content)

  @staticmethod
  def read_zip(stream):
    """Given some stream with zip data, reads and decompresses it into dict."""
    zip_file = zipfile.ZipFile(stream, 'r')
    try:
      return dict((i.filename, zip_file.read(i)) for i in zip_file.infolist())
    finally:
      zip_file.close()

  def test_require_absolute_root(self):
    # Absolute path is ok.
    zip_package.ZipPackage(self.temp_dir)
    # Relative path is not ok.
    with self.assertRaises(AssertionError):
      zip_package.ZipPackage('.')

  def test_require_absolute_file_paths(self):
    # Add some files to temp_dir.
    self.stage_files([
      'a.txt',
      'b.py',
      'c/c.txt',
    ])

    # Item to add -> method used to add it.
    cases = [
      ('a.txt', zip_package.ZipPackage.add_file),
      ('b.py', zip_package.ZipPackage.add_python_file),
      ('c', zip_package.ZipPackage.add_directory),
    ]

    for path, method in cases:
      pkg = zip_package.ZipPackage(self.temp_dir)
      # Absolute path is ok.
      method(pkg, os.path.join(self.temp_dir, path))
      # Relative path is not ok.
      with self.assertRaises(AssertionError):
        method(pkg, path)

  def test_added_files_are_under_root(self):
    # Add some files to temp_dir.
    self.stage_files([
      'a.txt',
      'p.py',
      'pkg/1.txt',
      'some_dir/2.txt',
    ])

    # Adding using |archive_path| should work.
    pkg = zip_package.ZipPackage(os.path.join(self.temp_dir, 'pkg'))
    pkg.add_file(os.path.join(self.temp_dir, 'a.txt'), '_a.txt')
    pkg.add_python_file(os.path.join(self.temp_dir, 'p.py'), '_p.py')
    pkg.add_directory(os.path.join(self.temp_dir, 'pkg'), '_pkg')

    # Adding without |archive_path| should fail.
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_file(os.path.join(self.temp_dir, 'a.txt'))
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_python_file(os.path.join(self.temp_dir, 'p.py'))
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_directory(os.path.join(self.temp_dir, 'a.txt'))

  def test_adding_missing_files(self):
    pkg = zip_package.ZipPackage(self.temp_dir)
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_file(os.path.join(self.temp_dir, 'im_not_here'))
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_python_file(os.path.join(self.temp_dir, 'im_not_here.py'))
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_directory(os.path.join(self.temp_dir, 'im_not_here_dir'))

  def test_adding_dir_as_file(self):
    # Create 'dir'.
    self.stage_files(['dir/keep'])
    # Try to add it as file, not a directory.
    pkg = zip_package.ZipPackage(self.temp_dir)
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_file(os.path.join(self.temp_dir, 'dir'))
    # Adding as directory works.
    pkg.add_directory(os.path.join(self.temp_dir, 'dir'))

  def test_adding_non_python_as_python(self):
    self.stage_files(['file.sh'])
    pkg = zip_package.ZipPackage(self.temp_dir)
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_python_file(os.path.join(self.temp_dir, 'file.sh'))

  def test_adding_py_instead_of_pyc(self):
    self.stage_files([
      'file.py',
      'file.pyo',
      'file.pyc',
    ])
    for alternative in ('file.pyc', 'file.pyo'):
      pkg = zip_package.ZipPackage(self.temp_dir)
      pkg.add_python_file(os.path.join(self.temp_dir, alternative))
      self.assertIn('file.py', pkg.files)

  def test_adding_same_file_twice(self):
    self.stage_files(['file'])
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_file(os.path.join(self.temp_dir, 'file'))
    with self.assertRaises(zip_package.ZipPackageError):
      pkg.add_file(os.path.join(self.temp_dir, 'file'))

  def test_add_directory(self):
    should_add = [
      'script.py',
      'a/1.txt',
      'a/2.txt',
      'a/b/3.txt',
      'a/script.py',
    ]
    should_ignore = [
      'script.pyc',
      'a/script.pyo',
      '.git/stuff',
      '.svn/stuff',
      'a/.svn/stuff',
      'a/b/.svn/stuff',
    ]
    # Add a whole set and verify only files from |should_add| were added.
    self.stage_files(should_add + should_ignore)
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_directory(self.temp_dir)
    self.assertEqual(set(pkg.files), set(should_add))

  def test_archive_path_is_respected(self):
    self.stage_files(['a', 'b.py', 'dir/c'])
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_file(os.path.join(self.temp_dir, 'a'), 'd1/a')
    pkg.add_python_file(os.path.join(self.temp_dir, 'b.py'), 'd2/b.py')
    pkg.add_directory(os.path.join(self.temp_dir, 'dir'), 'd3')
    self.assertEqual(set(pkg.files), set(['d1/a', 'd2/b.py', 'd3/c']))

  def test_add_buffer(self):
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_buffer('buf1', '')
    self.assertEqual(pkg.files, ['buf1'])
    # No unicode.
    with self.assertRaises(AssertionError):
      pkg.add_buffer('buf2', u'unicode')

  def test_zipping(self):
    data = {'a': '123', 'b/c': '456'}
    self.stage_files(data)
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_directory(self.temp_dir)
    # Test zip_into_buffer produces readable zip with same content.
    for compress in (True, False):
      buf = pkg.zip_into_buffer(compress=compress)
      self.assertEqual(data, self.read_zip(StringIO.StringIO(buf)))
    # Test zip_into_file produces readable zip with same content.
    for compress in (True, False):
      path = os.path.join(self.temp_dir, 'pkg.zip')
      pkg.zip_into_file(path, compress=compress)
      with open(path, 'rb') as f:
        self.assertEqual(data, self.read_zip(f))

  def test_repeatable_content(self):
    content = []
    for _ in range(2):
      # Build temp dir content from scratch.
      assert not os.listdir(self.temp_dir)
      self.stage_files({'a': '123', 'b': '456', 'c': '789'})
      # Zip it.
      pkg = zip_package.ZipPackage(self.temp_dir)
      pkg.add_directory(self.temp_dir)
      content.append(pkg.zip_into_buffer())
      # Clear everything.
      for name in os.listdir(self.temp_dir):
        os.remove(os.path.join(self.temp_dir, name))
    # Contents of both runs should match exactly.
    self.assertEqual(content[0], content[1])

  def test_running_from_zip(self):
    # Test assumes that it runs from a normal checkout, not a zip.
    self.assertFalse(zip_package.is_zipped_module(sys.modules[__name__]))
    self.assertIsNone(zip_package.get_module_zip_archive(sys.modules[__name__]))
    self.assertTrue(os.path.abspath(
        zip_package.get_main_script_path()).startswith(ROOT_DIR))

    # Build executable zip that calls same functions.
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_directory(os.path.join(ROOT_DIR, 'utils'), 'utils')
    pkg.add_buffer('__main__.py', '\n'.join([
      'import sys',
      '',
      'from utils import zip_package',
      '',
      'print zip_package.is_zipped_module(sys.modules[__name__])',
      'print zip_package.get_module_zip_archive(sys.modules[__name__])',
      'print zip_package.get_main_script_path()',
    ]))
    zip_file = os.path.join(self.temp_dir, 'out.zip')
    pkg.zip_into_file(zip_file)

    # Run the zip, validate results.
    actual = check_output([sys.executable, zip_file]).strip().splitlines()
    self.assertEqual(['True', zip_file, zip_file], actual)

  def test_extract_resource(self):
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_directory(os.path.join(ROOT_DIR, 'utils'), 'utils')
    pkg.add_buffer('cert.pem', 'Certificate\n')
    pkg.add_buffer('__main__.py', '\n'.join([
      'import sys',
      'from utils import zip_package',
      'print zip_package.extract_resource(sys.modules[__name__], \'cert.pem\')',
    ]))
    zip_file = os.path.join(self.temp_dir, 'out.zip')
    pkg.zip_into_file(zip_file)
    actual = check_output([sys.executable, zip_file]).strip()
    self.assertEqual(tempfile.gettempdir(), os.path.dirname(actual))
    basename = os.path.basename(actual)
    self.assertTrue(basename.startswith('.zip_pkg-'), actual)
    self.assertTrue(basename.endswith('-cert.pem'), actual)

  def test_extract_resource_temp_dir(self):
    pkg = zip_package.ZipPackage(self.temp_dir)
    pkg.add_directory(os.path.join(ROOT_DIR, 'utils'), 'utils')
    pkg.add_buffer('cert.pem', 'Certificate\n')
    pkg.add_buffer('__main__.py', '\n'.join([
      'import sys',
      'from utils import zip_package',
      'print zip_package.extract_resource(',
      '  sys.modules[__name__], \'cert.pem\', %r)' % self.temp_dir,
    ]))
    zip_file = os.path.join(self.temp_dir, 'out.zip')
    pkg.zip_into_file(zip_file)
    actual = check_output([sys.executable, zip_file]).strip()
    expected = os.path.join(
        self.temp_dir,
        '321690737f78d081937f88c3fd0e625dd48ae07d-cert.pem')
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
