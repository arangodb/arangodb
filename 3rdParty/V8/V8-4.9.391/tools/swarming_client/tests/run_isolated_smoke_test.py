#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import json
import logging
import os
import subprocess
import sys
import unittest

ROOT_DIR = unicode(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, ROOT_DIR)

import isolated_format
import run_isolated
from depot_tools import fix_encoding
from utils import file_path

import isolateserver_mock
import test_utils


CONTENTS = {
  'check_files.py': """if True:
      import os, sys
      ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
      expected = [
        'check_files.py', 'file1.txt', 'file1_copy.txt', 'file2.txt',
        'repeated_files.py',
      ]
      actual = sorted(os.listdir(ROOT_DIR))
      if expected != actual:
        print >> sys.stderr, 'Expected list doesn\\'t match:'
        print >> sys.stderr, '%s\\n%s' % (','.join(expected), ','.join(actual))
        sys.exit(1)
      # Check that file2.txt is in reality file3.txt.
      with open(os.path.join(ROOT_DIR, 'file2.txt'), 'rb') as f:
        if f.read() != 'File3\\n':
          print >> sys.stderr, 'file2.txt should be file3.txt in reality'
          sys.exit(2)
      print('Success')""",
  'file1.txt': 'File1\n',
  'file2.txt': 'File2.txt\n',
  'file3.txt': 'File3\n',
  'repeated_files.py': """if True:
      import os, sys
      expected = ['file1.txt', 'file1_copy.txt', 'repeated_files.py']
      actual = sorted(os.listdir(os.path.dirname(os.path.abspath(__file__))))
      if expected != actual:
        print >> sys.stderr, 'Expected list doesn\\'t match:'
        print >> sys.stderr, '%s\\n%s' % (','.join(expected), ','.join(actual))
        sys.exit(1)
      print('Success')""",
  'max_path.py': """if True:
      import os, sys
      prefix = u'\\\\\\\\?\\\\' if sys.platform == 'win32' else u''
      path = unicode(os.path.join(os.getcwd(), 'a' * 200, 'b' * 200))
      with open(prefix + path, 'rb') as f:
        actual = f.read()
        if actual != 'File1\\n':
          print >> sys.stderr, 'Unexpected content: %s' % actual
          sys.exit(1)
      print('Success')""",
}


def file_meta(filename):
  return {
    'h': isolateserver_mock.hash_content(CONTENTS[filename]),
    's': len(CONTENTS[filename]),
  }


CONTENTS['download.isolated'] = json.dumps(
    {
      'command': ['python', 'repeated_files.py'],
      'files': {
        'file1.txt': file_meta('file1.txt'),
        'file1_symlink.txt': {'l': 'files1.txt'},
        'new_folder/file1.txt': file_meta('file1.txt'),
        'repeated_files.py': file_meta('repeated_files.py'),
      },
    })


CONTENTS['file_with_size.isolated'] = json.dumps(
    {
      'command': [ 'python', '-V' ],
      'files': {'file1.txt': file_meta('file1.txt')},
      'read_only': 1,
    })


CONTENTS['manifest1.isolated'] = json.dumps(
    {'files': {'file1.txt': file_meta('file1.txt')}})


CONTENTS['manifest2.isolated'] = json.dumps(
    {
      'files': {'file2.txt': file_meta('file2.txt')},
      'includes': [
        isolateserver_mock.hash_content(CONTENTS['manifest1.isolated']),
      ],
    })


CONTENTS['max_path.isolated'] = json.dumps(
    {
      'command': ['python', 'max_path.py'],
      'files': {
        'a' * 200 + '/' + 'b' * 200: file_meta('file1.txt'),
        'max_path.py': file_meta('max_path.py'),
      },
    })


CONTENTS['repeated_files.isolated'] = json.dumps(
    {
      'command': ['python', 'repeated_files.py'],
      'files': {
        'file1.txt': file_meta('file1.txt'),
        'file1_copy.txt': file_meta('file1.txt'),
        'repeated_files.py': file_meta('repeated_files.py'),
      },
    })


CONTENTS['check_files.isolated'] = json.dumps(
    {
      'command': ['python', 'check_files.py'],
      'files': {
        'check_files.py': file_meta('check_files.py'),
        # Mapping another file.
        'file2.txt': file_meta('file3.txt'),
      },
      'includes': [
        isolateserver_mock.hash_content(CONTENTS[i])
        for i in ('manifest2.isolated', 'repeated_files.isolated')
      ]
    })


def list_files_tree(directory):
  """Returns the list of all the files in a tree."""
  actual = []
  for root, _dirs, files in os.walk(directory):
    actual.extend(os.path.join(root, f)[len(directory)+1:] for f in files)
  return sorted(actual)


def read_content(filepath):
  with open(filepath, 'rb') as f:
    return f.read()


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def tree_modes(root):
  """Returns the dict of files in a directory with their filemode.

  Includes |root| as '.'.
  """
  out = {}
  offset = len(root.rstrip('/\\')) + 1
  out['.'] = oct(os.stat(root).st_mode)
  for dirpath, dirnames, filenames in os.walk(root):
    for filename in filenames:
      p = os.path.join(dirpath, filename)
      out[p[offset:]] = oct(os.stat(p).st_mode)
    for dirname in dirnames:
      p = os.path.join(dirpath, dirname)
      out[p[offset:]] = oct(os.stat(p).st_mode)
  return out


class RunIsolatedTest(unittest.TestCase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    self.tempdir = run_isolated.make_temp_dir(
        u'run_isolated_smoke_test', ROOT_DIR)
    logging.debug(self.tempdir)
    # run_isolated.zip executable package.
    self.run_isolated_zip = os.path.join(self.tempdir, 'run_isolated.zip')
    run_isolated.get_as_zip_package().zip_into_file(
        self.run_isolated_zip, compress=False)
    # The run_isolated local cache.
    self.cache = os.path.join(self.tempdir, 'cache')
    self.server = isolateserver_mock.MockIsolateServer()

  def tearDown(self):
    try:
      self.server.close_start()
      file_path.rmtree(self.tempdir)
      self.server.close_end()
    finally:
      super(RunIsolatedTest, self).tearDown()

  def _run(self, args):
    cmd = [sys.executable, self.run_isolated_zip]
    cmd.extend(args)
    pipe = subprocess.PIPE
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd,
        stdout=pipe,
        stderr=pipe,
        universal_newlines=True,
        cwd=self.tempdir)
    out, err = proc.communicate()
    return out, err, proc.returncode

  def _store_isolated(self, data):
    """Stores an isolated file and returns its hash."""
    return self.server.add_content('default', json.dumps(data, sort_keys=True))

  def _store(self, filename):
    """Stores a test data file in the table and returns its hash."""
    return self.server.add_content('default', CONTENTS[filename])

  def _cmd_args(self, hash_value):
    """Generates the standard arguments used with |hash_value| as the hash.

    Returns a list of the required arguments.
    """
    return [
      '--isolated', hash_value,
      '--cache', self.cache,
      '--isolate-server', self.server.url,
      '--namespace', 'default',
    ]

  def assertTreeModes(self, root, expected):
    """Compares the file modes of everything in |root| with |expected|.

    Arguments:
      root: directory to list its tree.
      expected: dict(relpath: (linux_mode, mac_mode, win_mode)) where each mode
                is the expected file mode on this OS. For practical purposes,
                linux is "anything but OSX or Windows". The modes should be
                ints.
    """
    actual = tree_modes(root)
    if sys.platform == 'win32':
      index = 2
    elif sys.platform == 'darwin':
      index = 1
    else:
      index = 0
    expected_mangled = dict((k, oct(v[index])) for k, v in expected.iteritems())
    self.assertEqual(expected_mangled, actual)

  def test_normal(self):
    # Loads the .isolated from the store as a hash.
    # Load an isolated file with the same content (same SHA-1), listed under two
    # different names and ensure both are created.
    isolated_hash = self._store('repeated_files.isolated')
    expected = [
      'state.json',
      isolated_hash,
      self._store('file1.txt'),
      self._store('repeated_files.py'),
    ]

    out, err, returncode = self._run(self._cmd_args(isolated_hash))
    self.assertEqual('', err)
    self.assertEqual('Success\n', out, out)
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(set(expected)), actual)

  def test_max_path(self):
    # Make sure we can map and delete a tree that has paths longer than
    # MAX_PATH.
    isolated_hash = self._store('max_path.isolated')
    expected = [
      'state.json',
      isolated_hash,
      self._store('file1.txt'),
      self._store('max_path.py'),
    ]
    out, err, returncode = self._run(self._cmd_args(isolated_hash))
    self.assertEqual('', err)
    self.assertEqual('Success\n', out, out)
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(set(expected)), actual)

  def test_fail_empty_isolated(self):
    isolated_hash = self._store_isolated({})
    expected = ['state.json', isolated_hash]
    out, err, returncode = self._run(self._cmd_args(isolated_hash))
    self.assertEqual('', out)
    self.assertIn('No command to run\n', err)
    self.assertEqual(1, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(expected), actual)

  def test_includes(self):
    # Loads an .isolated that includes another one.

    # References manifest2.isolated and repeated_files.isolated. Maps file3.txt
    # as file2.txt.
    isolated_hash = self._store('check_files.isolated')
    expected = [
      'state.json',
      isolated_hash,
      self._store('check_files.py'),
      self._store('file1.txt'),
      self._store('file3.txt'),
      # Maps file1.txt.
      self._store('manifest1.isolated'),
      # References manifest1.isolated. Maps file2.txt but it is overriden.
      self._store('manifest2.isolated'),
      self._store('repeated_files.py'),
      self._store('repeated_files.isolated'),
    ]
    out, err, returncode = self._run(self._cmd_args(isolated_hash))
    self.assertEqual('', err)
    self.assertEqual('Success\n', out)
    self.assertEqual(0, returncode)
    actual = list_files_tree(self.cache)
    self.assertEqual(sorted(expected), actual)

  def _test_corruption_common(self, new_content):
    isolated_hash = self._store('file_with_size.isolated')
    file1_hash = self._store('file1.txt')

    # Run the test once to generate the cache.
    _out, _err, returncode = self._run(self._cmd_args(isolated_hash))
    self.assertEqual(0, returncode)
    expected = {
      '.': (040707, 040707, 040777),
      'state.json': (0100606, 0100606, 0100666),
      # The reason for 0100666 on Windows is that the file node had to be
      # modified to delete the hardlinked node. The read only bit is reset on
      # load.
      file1_hash: (0100400, 0100400, 0100666),
      isolated_hash: (0100400, 0100400, 0100444),
    }
    self.assertTreeModes(self.cache, expected)

    # Modify one of the files in the cache to be invalid.
    cached_file_path = os.path.join(self.cache, file1_hash)
    previous_mode = os.stat(cached_file_path).st_mode
    os.chmod(cached_file_path, 0600)
    write_content(cached_file_path, new_content)
    os.chmod(cached_file_path, previous_mode)
    logging.info('Modified %s', cached_file_path)
    # Ensure that the cache has an invalid file.
    self.assertNotEqual(CONTENTS['file1.txt'], read_content(cached_file_path))

    # Rerun the test and make sure the cache contains the right file afterwards.
    _out, _err, returncode = self._run(self._cmd_args(isolated_hash))
    self.assertEqual(0, returncode)
    expected = {
      '.': (040700, 040700, 040777),
      'state.json': (0100600, 0100600, 0100666),
      file1_hash: (0100400, 0100400, 0100666),
      isolated_hash: (0100400, 0100400, 0100444),
    }
    self.assertTreeModes(self.cache, expected)
    return cached_file_path

  def test_corrupted_cache_entry_different_size(self):
    # Test that an entry with an invalid file size properly gets removed and
    # fetched again. This test case also check for file modes.
    cached_file_path = self._test_corruption_common(
        CONTENTS['file1.txt'] + ' now invalid size')
    self.assertEqual(CONTENTS['file1.txt'], read_content(cached_file_path))

  def test_corrupted_cache_entry_same_size(self):
    # Test that an entry with an invalid file content but same size is NOT
    # detected property.
    cached_file_path = self._test_corruption_common(
        CONTENTS['file1.txt'][:-1] + ' ')
    # TODO(maruel): This corruption is NOT detected.
    # This needs to be fixed.
    self.assertNotEqual(CONTENTS['file1.txt'], read_content(cached_file_path))


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  test_utils.main()
