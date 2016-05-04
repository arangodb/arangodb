#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import hashlib
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import isolated_format
import test_utils
from utils import file_path

# Ensure that the testing machine has access to this server.
ISOLATE_SERVER = 'https://isolateserver.appspot.com/'


CONTENTS = {
  'empty_file.txt': '',
  'small_file.txt': 'small file\n',
  # TODO(maruel): symlinks.
}


class IsolateServerArchiveSmokeTest(unittest.TestCase):
  def setUp(self):
    super(IsolateServerArchiveSmokeTest, self).setUp()
    # The namespace must end in '-gzip' since all files are now compressed
    # before being uploaded.
    # TODO(maruel): This should not be leaked to the client. It's a
    # transport/storage detail.
    self.namespace = ('temporary' + str(long(time.time())).split('.', 1)[0]
                      + '-gzip')
    self.tempdir = tempfile.mkdtemp(prefix=u'isolateserver')
    self.rootdir = os.path.join(self.tempdir, 'rootdir')
    self.test_data = os.path.join(self.tempdir, 'test_data')
    test_utils.make_tree(self.test_data, CONTENTS)

  def tearDown(self):
    try:
      file_path.rmtree(self.tempdir)
    finally:
      super(IsolateServerArchiveSmokeTest, self).tearDown()

  def _run(self, args):
    """Runs isolateserver.py."""
    cmd = [
        sys.executable,
        os.path.join(ROOT_DIR, 'isolateserver.py'),
    ]
    cmd.extend(args)
    cmd.extend(
        [
          '--isolate-server', ISOLATE_SERVER,
          '--namespace', self.namespace
        ])
    if '-v' in sys.argv:
      cmd.append('--verbose')
      subprocess.check_call(cmd)
    else:
      subprocess.check_output(cmd)

  def _archive_given_files(self, files):
    """Given a list of files, call isolateserver.py with them. Then
    verify they are all on the server."""
    files = [os.path.join(self.test_data, filename) for filename in files]
    self._run(['archive'] + files)
    self._download_given_files(files)

  def _download_given_files(self, files):
    """Tries to download the files from the server."""
    args = ['download', '--target', self.rootdir]
    file_hashes = [isolated_format.hash_file(f, hashlib.sha1) for f in files]
    for f in file_hashes:
      args.extend(['--file', f, f])
    self._run(args)
    # Assert the files are present.
    actual = [
        isolated_format.hash_file(os.path.join(self.rootdir, f), hashlib.sha1)
        for f in os.listdir(self.rootdir)
    ]
    self.assertEqual(sorted(file_hashes), sorted(actual))

  def test_archive_empty_file(self):
    self._archive_given_files(['empty_file.txt'])

  def test_archive_small_file(self):
    self._archive_given_files(['small_file.txt'])

  def test_archive_huge_file(self):
    # Create a file over 2gbs.
    name = '2.1gb.7z'
    with open(os.path.join(self.test_data, name), 'wb') as f:
      # Write 2.1gb.
      data = os.urandom(1024)
      for _ in xrange(2150 * 1024):
        f.write(data)
    self._archive_given_files([name])

  if sys.maxsize == (2**31) - 1:
    def test_archive_multiple_huge_file(self):
      # Create multiple files over 2.5gb. This test exists to stress the virtual
      # address space on 32 bits systems
      files = []
      for i in xrange(5):
        name = '512mb_%d.7z' % i
        files.append(name)
        with open(os.path.join(self.test_data, name), 'wb') as f:
          # Write 512mb.
          data = os.urandom(1024)
          for _ in xrange(512 * 1024):
            f.write(data)
      self._archive_given_files(files)


if __name__ == '__main__':
  if len(sys.argv) > 1 and sys.argv[1].startswith('http'):
    ISOLATE_SERVER = sys.argv.pop(1).rstrip('/') + '/'
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
