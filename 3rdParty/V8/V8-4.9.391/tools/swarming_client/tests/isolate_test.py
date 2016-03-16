#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import cStringIO
import hashlib
import json
import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT_DIR = unicode(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from depot_tools import fix_encoding
import auth
import isolate
import isolate_format
import isolated_format
import isolateserver
from utils import file_path
from utils import logging_utils
from utils import tools
import test_utils

ALGO = hashlib.sha1


NO_RUN_ISOLATE = {
  'tests/isolate/files1/subdir/42.txt':
      'the answer to life the universe and everything\n',
  'tests/isolate/files1/test_file1.txt': 'Foo\n',
  'tests/isolate/files1/test_file2.txt': 'Bar\n',
  'tests/isolate/no_run.isolate':
    """{
        # Includes itself.
      'variables': {'files': ['no_run.isolate', 'files1/']},
    }""",
}

SPLIT_ISOLATE = {
  'tests/isolate/files1/subdir/42.txt':
      'the answer to life the universe and everything',
  'tests/isolate/split.isolate':
    """{
      'variables': {
        'command': ['python', 'split.py'],
        'files': [
          '<(DEPTH)/split.py',
          '<(PRODUCT_DIR)/subdir/42.txt',
          'test/data/foo.txt',
        ],
      },
    }""",
  'tests/isolate/split.py': "import sys; sys.exit(1)",
  'tests/isolate/test/data/foo.txt': 'Split',
}


TOUCH_ROOT_ISOLATE = {
  'tests/isolate/touch_root.isolate':
    """{
      'conditions': [
        ['(OS=="linux" and chromeos==1) or ((OS=="mac" or OS=="win") and '
         'chromeos==0)', {
          'variables': {
            'command': ['python', 'touch_root.py'],
            'files': ['../../at_root', 'touch_root.py'],
          },
        }],
      ],
    }""",
  'tests/isolate/touch_root.py':
    "def main():\n"
    "  import os, sys\n"
    "  print('child_touch_root: Verify the relative directories')\n"
    "  root_dir = os.path.dirname(os.path.abspath(__file__))\n"
    "  parent_dir, base = os.path.split(root_dir)\n"
    "  parent_dir, base2 = os.path.split(parent_dir)\n"
    "  if base != 'isolate' or base2 != 'tests':\n"
    "    print 'Invalid root dir %s' % root_dir\n"
    "    return 4\n"
    "  content = open(os.path.join(parent_dir, 'at_root'), 'r').read()\n"
    "  return int(content != 'foo')\n"
    "\n"
    "if __name__ == '__main__':\n"
    "  sys.exit(main())\n",
  'at_root': 'foo',
}


class IsolateBase(auto_stub.TestCase):
  def setUp(self):
    super(IsolateBase, self).setUp()
    self.mock(auth, 'ensure_logged_in', lambda _: None)
    self.old_cwd = os.getcwd()
    self.cwd = file_path.get_native_path_case(
        unicode(tempfile.mkdtemp(prefix=u'isolate_')))
    # Everything should work even from another directory.
    os.chdir(self.cwd)
    self.mock(
        logging_utils.OptionParserWithLogging, 'logger_root',
        logging.Logger('unittest'))

  def tearDown(self):
    try:
      os.chdir(self.old_cwd)
      file_path.rmtree(self.cwd)
    finally:
      super(IsolateBase, self).tearDown()


class IsolateTest(IsolateBase):
  def test_savedstate_load_minimal(self):
    # The file referenced by 'isolate_file' must exist even if its content is
    # not read.
    open(os.path.join(self.cwd, 'fake.isolate'), 'wb').close()
    values = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'isolate_file': 'fake.isolate',
    }
    expected = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'config_variables': {},
      'command': [],
      'extra_variables': {},
      'files': {},
      'isolate_file': 'fake.isolate',
      'path_variables': {},
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    saved_state = isolate.SavedState.load(values, self.cwd)
    self.assertEqual(expected, saved_state.flatten())

  def test_savedstate_load(self):
    # The file referenced by 'isolate_file' must exist even if its content is
    # not read.
    open(os.path.join(self.cwd, 'fake.isolate'), 'wb').close()
    values = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'config_variables': {},
      'extra_variables': {
        'foo': 42,
      },
      'isolate_file': 'fake.isolate',
    }
    expected = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': [],
      'config_variables': {},
      'extra_variables': {
        'foo': 42,
      },
      'files': {},
      'isolate_file': 'fake.isolate',
      'path_variables': {},
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    saved_state = isolate.SavedState.load(values, self.cwd)
    self.assertEqual(expected, saved_state.flatten())

  def test_variable_arg(self):
    parser = optparse.OptionParser()
    isolate.add_isolate_options(parser)
    options, args = parser.parse_args(
        ['--config-variable', 'Foo', 'bar',
          '--path-variable', 'Baz=sub=string',
          '--extra-variable', 'biz', 'b uz=a'])
    isolate.process_isolate_options(parser, options, require_isolated=False)

    expected_path = {
      'Baz': 'sub=string',
    }
    expected_config = {
      'Foo': 'bar',
    }
    expected_extra = {
      'biz': 'b uz=a',
      'EXECUTABLE_SUFFIX': '.exe' if sys.platform == 'win32' else '',
    }
    self.assertEqual(expected_path, options.path_variables)
    self.assertEqual(expected_config, options.config_variables)
    self.assertEqual(expected_extra, options.extra_variables)
    self.assertEqual([], args)

  def test_variable_arg_fail(self):
    parser = optparse.OptionParser()
    isolate.add_isolate_options(parser)
    self.mock(sys, 'stderr', cStringIO.StringIO())
    with self.assertRaises(SystemExit):
      parser.parse_args(['--config-variable', 'Foo'])

  def test_blacklist_default(self):
    ok = [
      '.git2',
      '.pyc',
      '.swp',
      'allo.git',
      'foo',
    ]
    blocked = [
      '.git',
      os.path.join('foo', '.git'),
      'foo.pyc',
      'bar.swp',
    ]
    blacklist = tools.gen_blacklist(isolateserver.DEFAULT_BLACKLIST)
    for i in ok:
      self.assertFalse(blacklist(i), i)
    for i in blocked:
      self.assertTrue(blacklist(i), i)

  def test_blacklist_custom(self):
    ok = [
      '.run_test_cases',
      'testserver.log2',
    ]
    blocked = [
      'foo.run_test_cases',
      'testserver.log',
      os.path.join('foo', 'testserver.log'),
    ]
    blacklist = tools.gen_blacklist([r'^.+\.run_test_cases$', r'^.+\.log$'])
    for i in ok:
      self.assertFalse(blacklist(i), i)
    for i in blocked:
      self.assertTrue(blacklist(i), i)

  def test_read_only(self):
    isolate_file = os.path.join(self.cwd, 'fake.isolate')
    isolate_content = {
      'variables': {
        'read_only': 0,
      },
    }
    tools.write_json(isolate_file, isolate_content, False)
    expected = {
      'algo': 'sha-1',
      'files': {},
      'read_only': 0,
      'relative_cwd': '.',
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    complete_state = isolate.CompleteState(None, isolate.SavedState(self.cwd))
    complete_state.load_isolate(
        unicode(self.cwd), unicode(isolate_file), {}, {}, {}, None, False)
    self.assertEqual(expected, complete_state.saved_state.to_isolated())


class IsolateLoad(IsolateBase):
  def setUp(self):
    super(IsolateLoad, self).setUp()
    self.directory = tempfile.mkdtemp(prefix=u'isolate_')
    self.isolate_dir = os.path.join(self.directory, u'isolate')
    self.isolated_dir = os.path.join(self.directory, u'isolated')
    os.mkdir(self.isolated_dir, 0700)

  def tearDown(self):
    try:
      file_path.rmtree(self.directory)
    finally:
      super(IsolateLoad, self).tearDown()

  def _get_option(self, *isolatepath):
    isolate_file = os.path.join(self.isolate_dir, *isolatepath)
    class Options(object):
      isolated = os.path.join(self.isolated_dir, 'foo.isolated')
      outdir = os.path.join(self.directory, 'outdir')
      isolate = isolate_file
      blacklist = list(isolateserver.DEFAULT_BLACKLIST)
      path_variables = {}
      config_variables = {
        'OS': 'linux',
        'chromeos': 1,
      }
      extra_variables = {'foo': 'bar'}
      ignore_broken_items = False
    return Options()

  def _cleanup_isolated(self, expected_isolated):
    """Modifies isolated to remove the non-deterministic parts."""
    if sys.platform == 'win32':
      # 'm' are not saved in windows.
      for values in expected_isolated['files'].itervalues():
        self.assertTrue(values.pop('m'))

  def _cleanup_saved_state(self, actual_saved_state):
    for item in actual_saved_state['files'].itervalues():
      self.assertTrue(item.pop('t'))

  def make_tree(self, contents):
    test_utils.make_tree(self.isolate_dir, contents)

  def size(self, *args):
    return os.stat(os.path.join(self.isolate_dir, *args)).st_size

  def hash_file(self, *args):
    p = os.path.join(*args)
    if not os.path.isabs(p):
      p = os.path.join(self.isolate_dir, p)
    return isolated_format.hash_file(p, ALGO)

  def test_load_stale_isolated(self):
    # Data to be loaded in the .isolated file. Do not create a .state file.
    self.make_tree(TOUCH_ROOT_ISOLATE)
    input_data = {
      'command': ['python'],
      'files': {
        'foo': {
          "m": 0640,
          "h": "invalid",
          "s": 538,
          "t": 1335146921,
        },
        os.path.join('tests', 'isolate', 'touch_root.py'): {
          "m": 0750,
          "h": "invalid",
          "s": 538,
          "t": 1335146921,
        },
      },
    }
    options = self._get_option('tests', 'isolate', 'touch_root.isolate')
    tools.write_json(options.isolated, input_data, False)

    # A CompleteState object contains two parts:
    # - Result instance stored in complete_state.isolated, corresponding to the
    #   .isolated file, is what is read by run_test_from_archive.py.
    # - SavedState instance stored in compelte_state.saved_state,
    #   corresponding to the .state file, which is simply to aid the developer
    #   when re-running the same command multiple times and contain
    #   discardable information.
    complete_state = isolate.load_complete_state(options, self.cwd, None, False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated = {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
        u'at_root': {
          'm': 0600,
          'h': self.hash_file('at_root'),
          's': self.size('at_root'),
        },
      },
      'read_only': 1,
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    isolate_file = os.path.join(
        self.isolate_dir, 'tests', 'isolate', 'touch_root.isolate')
    expected_saved_state = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': 'linux',
        'chromeos': options.config_variables['chromeos'],
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
        u'at_root': {
          'm': 0600,
          'h': self.hash_file('at_root'),
          's': self.size('at_root'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'path_variables': {},
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'root_dir': file_path.get_native_path_case(self.isolate_dir),
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)

  def test_subdir(self):
    # The resulting .isolated file will be missing ../../at_root. It is
    # because this file is outside the --subdir parameter.
    self.make_tree(TOUCH_ROOT_ISOLATE)
    options = self._get_option('tests', 'isolate', 'touch_root.isolate')
    complete_state = isolate.load_complete_state(
        options, self.cwd, os.path.join('tests', 'isolate'), False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated =  {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'read_only': 1,
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    isolate_file = os.path.join(
        self.isolate_dir, 'tests', 'isolate', 'touch_root.isolate')
    expected_saved_state = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': 'linux',
        'chromeos': 1,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'path_variables': {},
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'root_dir': file_path.get_native_path_case(self.isolate_dir),
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)

  def test_subdir_variable(self):
    # the resulting .isolated file will be missing ../../at_root. it is
    # because this file is outside the --subdir parameter.
    self.make_tree(TOUCH_ROOT_ISOLATE)
    options = self._get_option('tests', 'isolate', 'touch_root.isolate')
    # Path variables are keyed on the directory containing the .isolate file.
    options.path_variables['TEST_ISOLATE'] = '.'
    # Note that options.isolated is in self.directory, which is a temporary
    # directory.
    complete_state = isolate.load_complete_state(
        options, os.path.join(self.isolate_dir, 'tests', 'isolate'),
        '<(TEST_ISOLATE)', False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated =  {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'read_only': 1,
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    # It is important to note:
    # - the root directory is self.isolate_dir.
    # - relative_cwd is tests/isolate.
    # - TEST_ISOLATE is based of relative_cwd, so it represents tests/isolate.
    # - anything outside TEST_ISOLATE was not included in the 'files' section.
    isolate_file = os.path.join(
        self.isolate_dir, 'tests', 'isolate', 'touch_root.isolate')
    expected_saved_state = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': 'linux',
        'chromeos': 1,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'path_variables': {
        'TEST_ISOLATE': '.',
      },
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'root_dir': file_path.get_native_path_case(self.isolate_dir),
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)

  def test_variable_not_exist(self):
    self.make_tree(TOUCH_ROOT_ISOLATE)
    options = self._get_option('tests', 'isolate', 'touch_root.isolate')
    options.path_variables['PRODUCT_DIR'] = os.path.join(u'tests', u'isolate')
    native_cwd = file_path.get_native_path_case(unicode(self.cwd))
    try:
      isolate.load_complete_state(options, self.cwd, None, False)
      self.fail()
    except isolate.ExecutionError, e:
      self.assertEqual(
          'PRODUCT_DIR=%s is not a directory' %
            os.path.join(native_cwd, 'tests', 'isolate'),
          e.args[0])

  def test_variable(self):
    self.make_tree(TOUCH_ROOT_ISOLATE)
    options = self._get_option('tests', 'isolate', 'touch_root.isolate')
    options.path_variables['PRODUCT_DIR'] = os.path.join('tests', 'isolate')
    complete_state = isolate.load_complete_state(
        options, self.isolate_dir, None, False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated =  {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        u'at_root': {
          'm': 0600,
          'h': self.hash_file('at_root'),
          's': self.size('at_root'),
        },
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'read_only': 1,
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)
    isolate_file = os.path.join(
        self.isolate_dir, 'tests', 'isolate', 'touch_root.isolate')
    expected_saved_state = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': 'linux',
        'chromeos': 1,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        u'at_root': {
          'm': 0600,
          'h': self.hash_file('at_root'),
          's': self.size('at_root'),
        },
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 0700,
          'h': self.hash_file('tests', 'isolate', 'touch_root.py'),
          's': self.size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'path_variables': {
        'PRODUCT_DIR': '.',
      },
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'root_dir': file_path.get_native_path_case(self.isolate_dir),
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)
    self.assertEqual([], os.listdir(self.isolated_dir))

  def test_root_dir_because_of_variable(self):
    # Ensures that load_isolate() works even when path variables have deep root
    # dirs. The end result is similar to touch_root.isolate, except that
    # no_run.isolate doesn't reference '..' at all.
    #
    # A real world example would be PRODUCT_DIR=../../out/Release but nothing in
    # this directory is mapped.
    #
    # Imagine base/base_unittests.isolate would not map anything in
    # PRODUCT_DIR. In that case, the automatically determined root dir is
    # src/base, since nothing outside this directory is mapped.
    self.make_tree(NO_RUN_ISOLATE)
    options = self._get_option('tests', 'isolate', 'no_run.isolate')
    # Any directory outside <self.isolate_dir>/tests/isolate.
    options.path_variables['PRODUCT_DIR'] = 'third_party'
    os.mkdir(os.path.join(self.isolate_dir, 'third_party'), 0700)
    complete_state = isolate.load_complete_state(
        options, self.isolate_dir, None, False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated = {
      'algo': 'sha-1',
      'files': {
        os.path.join(u'tests', 'isolate', 'files1', 'subdir', '42.txt'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt'),
          's': self.size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file1.txt'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'files1', 'test_file1.txt'),
          's': self.size('tests', 'isolate', 'files1', 'test_file1.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file2.txt'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'files1', 'test_file2.txt'),
          's': self.size('tests', 'isolate', 'files1', 'test_file2.txt'),
        },
        os.path.join(u'tests', 'isolate', 'no_run.isolate'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'no_run.isolate'),
          's': self.size('tests', 'isolate', 'no_run.isolate'),
        },
      },
      'read_only': 1,
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)
    isolate_file = os.path.join(
        self.isolate_dir, 'tests', 'isolate', 'no_run.isolate')
    expected_saved_state = {
      'OS': sys.platform,
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': [],
      'config_variables': {
        'OS': 'linux',
        'chromeos': 1,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'files1', 'subdir', '42.txt'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt'),
          's': self.size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file1.txt'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'files1', 'test_file1.txt'),
          's': self.size('tests', 'isolate', 'files1', 'test_file1.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file2.txt'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'files1', 'test_file2.txt'),
          's': self.size('tests', 'isolate', 'files1', 'test_file2.txt'),
        },
        os.path.join(u'tests', 'isolate', 'no_run.isolate'): {
          'm': 0600,
          'h': self.hash_file('tests', 'isolate', 'no_run.isolate'),
          's': self.size('tests', 'isolate', 'no_run.isolate'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'path_variables': {
        'PRODUCT_DIR': os.path.join(u'..', '..', 'third_party'),
      },
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'root_dir': file_path.get_native_path_case(self.isolate_dir),
      'version': isolate.SavedState.EXPECTED_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)
    self.assertEqual([], os.listdir(self.isolated_dir))

  def test_chromium_split(self):
    # Create an .isolate file and a tree of random stuff.
    self.make_tree(SPLIT_ISOLATE)
    options = self._get_option('tests', 'isolate', 'split.isolate')
    options.path_variables = {
      'DEPTH': '.',
      'PRODUCT_DIR': os.path.join('files1'),
    }
    options.config_variables = {
      'OS': 'linux',
    }
    complete_state = isolate.load_complete_state(
        options, os.path.join(self.isolate_dir, 'tests', 'isolate'), None,
        False)
    # By saving the files, it forces splitting the data up.
    complete_state.save_files()

    actual_isolated_master = tools.read_json(
        os.path.join(self.isolated_dir, 'foo.isolated'))
    expected_isolated_master = {
      u'algo': u'sha-1',
      u'command': [u'python', u'split.py'],
      u'files': {
        u'split.py': {
          u'm': 0700,
          u'h': unicode(self.hash_file('tests', 'isolate', 'split.py')),
          u's': self.size('tests', 'isolate', 'split.py'),
        },
      },
      u'includes': [
        unicode(self.hash_file(self.isolated_dir, 'foo.0.isolated')),
        unicode(self.hash_file(self.isolated_dir, 'foo.1.isolated')),
      ],
      u'read_only': 1,
      u'relative_cwd': u'.',
      u'version': unicode(isolated_format.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_isolated_master)
    self.assertEqual(expected_isolated_master, actual_isolated_master)

    actual_isolated_0 = tools.read_json(
        os.path.join(self.isolated_dir, 'foo.0.isolated'))
    expected_isolated_0 = {
      u'algo': u'sha-1',
      u'files': {
        os.path.join(u'test', 'data', 'foo.txt'): {
          u'm': 0600,
          u'h': unicode(
              self.hash_file('tests', 'isolate', 'test', 'data', 'foo.txt')),
          u's': self.size('tests', 'isolate', 'test', 'data', 'foo.txt'),
        },
      },
      u'version': unicode(isolated_format.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_isolated_0)
    self.assertEqual(expected_isolated_0, actual_isolated_0)

    actual_isolated_1 = tools.read_json(
        os.path.join(self.isolated_dir, 'foo.1.isolated'))
    expected_isolated_1 = {
      u'algo': u'sha-1',
      u'files': {
        os.path.join(u'files1', 'subdir', '42.txt'): {
          u'm': 0600,
          u'h': unicode(
              self.hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt')),
          u's': self.size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
      },
      u'version': unicode(isolated_format.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_isolated_1)
    self.assertEqual(expected_isolated_1, actual_isolated_1)

    actual_saved_state = tools.read_json(
        isolate.isolatedfile_to_state(options.isolated))
    isolated_base = unicode(os.path.basename(options.isolated))
    isolate_file = os.path.join(
        self.isolate_dir, 'tests', 'isolate', 'split.isolate')
    expected_saved_state = {
      u'OS': unicode(sys.platform),
      u'algo': u'sha-1',
      u'child_isolated_files': [
        isolated_base[:-len('.isolated')] + '.0.isolated',
        isolated_base[:-len('.isolated')] + '.1.isolated',
      ],
      u'command': [u'python', u'split.py'],
      u'config_variables': {
        u'OS': u'linux',
      },
      u'extra_variables': {
        u'foo': u'bar',
      },
      u'files': {
        os.path.join(u'files1', 'subdir', '42.txt'): {
          u'm': 0600,
          u'h': unicode(
              self.hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt')),
          u's': self.size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
        u'split.py': {
          u'm': 0700,
          u'h': unicode(self.hash_file('tests', 'isolate', 'split.py')),
          u's': self.size('tests', 'isolate', 'split.py'),
        },
        os.path.join(u'test', 'data', 'foo.txt'): {
          u'm': 0600,
          u'h': unicode(
              self.hash_file('tests', 'isolate', 'test', 'data', 'foo.txt')),
          u's': self.size('tests', 'isolate', 'test', 'data', 'foo.txt'),
        },
      },
      u'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          unicode(os.path.dirname(options.isolated))),
      u'path_variables': {
        u'DEPTH': u'.',
        u'PRODUCT_DIR': u'files1',
      },
      u'relative_cwd': u'.',
      u'root_dir': file_path.get_native_path_case(
          os.path.dirname(isolate_file)),
      u'version': unicode(isolate.SavedState.EXPECTED_VERSION),
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)
    self.assertEqual(
        [
          'foo.0.isolated', 'foo.1.isolated',
          'foo.isolated', 'foo.isolated.state',
        ],
        sorted(os.listdir(self.isolated_dir)))

  def test_load_isolate_include_command(self):
    # Ensure that using a .isolate that includes another one in a different
    # directory will lead to the proper relative directory. See
    # test_load_with_includes_with_commands in isolate_format_test.py as
    # reference.

    # Exactly the same thing as in isolate_format_test.py
    isolate1 = {
      'conditions': [
        ['OS=="amiga" or OS=="win"', {
          'variables': {
            'command': [
              'foo', 'amiga_or_win',
            ],
          },
        }],
        ['OS=="linux"', {
          'variables': {
            'command': [
              'foo', 'linux',
            ],
            'files': [
              'file_linux',
            ],
          },
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'files': [
              'file_non_linux',
            ],
          },
        }],
      ],
    }
    isolate2 = {
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'linux_or_mac',
            ],
            'files': [
              'other/file',
            ],
          },
        }],
      ],
    }
    # Do not define command in isolate3, otherwise commands in the other
    # included .isolated will be ignored.
    isolate3 = {
      'includes': [
        '../1/isolate1.isolate',
        '2/isolate2.isolate',
      ],
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'files': [
              'file_amiga',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'files': [
              'file_mac',
            ],
          },
        }],
      ],
    }

    def test_with_os(
        config_os, files_to_create, expected_files, command, relative_cwd):
      """Creates a tree of files in a subdirectory for testing and test this
      set of conditions.
      """
      directory = os.path.join(unicode(self.directory), config_os)
      os.mkdir(directory)
      isolate_dir = os.path.join(directory, u'isolate')
      isolate_dir_1 = os.path.join(isolate_dir, u'1')
      isolate_dir_3 = os.path.join(isolate_dir, u'3')
      isolate_dir_3_2 = os.path.join(isolate_dir_3, u'2')
      isolated_dir = os.path.join(directory, u'isolated')
      os.mkdir(isolated_dir)
      os.mkdir(isolate_dir)
      os.mkdir(isolate_dir_1)
      os.mkdir(isolate_dir_3)
      os.mkdir(isolate_dir_3_2)
      isolated = os.path.join(isolated_dir, u'foo.isolated')

      with open(os.path.join(isolate_dir_1, 'isolate1.isolate'), 'wb') as f:
        isolate_format.pretty_print(isolate1, f)
      with open(os.path.join(isolate_dir_3_2, 'isolate2.isolate'), 'wb') as f:
        isolate_format.pretty_print(isolate2, f)
      root_isolate = os.path.join(isolate_dir_3, 'isolate3.isolate')
      with open(root_isolate, 'wb') as f:
        isolate_format.pretty_print(isolate3, f)

      # Make all the touched files.
      mapping = {1: isolate_dir_1, 2: isolate_dir_3_2, 3: isolate_dir_3}
      for k, v in files_to_create.iteritems():
        f = os.path.join(mapping[k], v)
        base = os.path.dirname(f)
        if not os.path.isdir(base):
          os.mkdir(base)
        open(f, 'wb').close()

      c = isolate.CompleteState(isolated, isolate.SavedState(isolated_dir))
      config = {
        'OS': config_os,
      }
      c.load_isolate(
          unicode(self.cwd), root_isolate, {}, config, {}, None, False)
      # Note that load_isolate() doesn't retrieve the meta data about each file.
      expected = {
        'algo': 'sha-1',
        'command': command,
        'files': {
          unicode(f.replace('/', os.path.sep)):{} for f in expected_files
        },
        'read_only': 1,
        'relative_cwd': relative_cwd.replace('/', os.path.sep),
        'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      self.assertEqual(expected, c.saved_state.to_isolated())

    # root is .../isolate/.
    test_with_os(
        'amiga',
        {
          3: 'file_amiga',
        },
        (
          u'3/file_amiga',
        ),
        ['foo', 'amiga_or_win'],
        '1')
    # root is .../isolate/.
    test_with_os(
        'linux',
        {
          1: 'file_linux',
          2: 'other/file',
        },
        (
          u'1/file_linux',
          u'3/2/other/file',
        ),
        ['foo', 'linux_or_mac'],
        '3/2')
    # root is .../isolate/.
    test_with_os(
        'mac',
        {
          1: 'file_non_linux',
          2: 'other/file',
          3: 'file_mac',
        },
        (
          u'1/file_non_linux',
          u'3/2/other/file',
          u'3/file_mac',
        ),
        ['foo', 'linux_or_mac'],
        '3/2')
    # root is .../isolate/1/.
    test_with_os(
        'win',
        {
          1: 'file_non_linux',
        },
        (
          u'file_non_linux',
        ),
        ['foo', 'amiga_or_win'],
        '.')

  def test_load_isolate_include_command_and_variables(self):
    # Ensure that using a .isolate that includes another one in a different
    # directory will lead to the proper relative directory when using variables.
    # See test_load_with_includes_with_commands_and_variables in
    # isolate_format_test.py as reference.
    #
    # With path variables, 'cwd' is used instead of the path to the .isolate
    # file. So the files have to be set towards the cwd accordingly. While this
    # may seem surprising, this makes the whole thing work in the first place.

    # Almost exactly the same thing as in isolate_format_test.py plus the EXTRA
    # for better testing with variable replacement.
    isolate1 = {
      'conditions': [
        ['OS=="amiga" or OS=="win"', {
          'variables': {
            'command': [
              'foo', 'amiga_or_win', '<(PATH)', '<(EXTRA)',
            ],
          },
        }],
        ['OS=="linux"', {
          'variables': {
            'command': [
              'foo', 'linux', '<(PATH)', '<(EXTRA)',
            ],
            'files': [
              '<(PATH)/file_linux',
            ],
          },
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'files': [
              '<(PATH)/file_non_linux',
            ],
          },
        }],
      ],
    }
    isolate2 = {
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'linux_or_mac', '<(PATH)', '<(EXTRA)',
            ],
            'files': [
              '<(PATH)/other/file',
            ],
          },
        }],
      ],
    }
    isolate3 = {
      'includes': [
        '../1/isolate1.isolate',
        '2/isolate2.isolate',
      ],
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'files': [
              '<(PATH)/file_amiga',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'mac', '<(PATH)', '<(EXTRA)',
            ],
            'files': [
              '<(PATH)/file_mac',
            ],
          },
        }],
      ],
    }

    def test_with_os(config_os, expected_files, command, relative_cwd):
      """Creates a tree of files in a subdirectory for testing and test this
      set of conditions.
      """
      directory = os.path.join(unicode(self.directory), config_os)
      os.mkdir(directory)
      cwd = os.path.join(unicode(self.cwd), config_os)
      os.mkdir(cwd)
      isolate_dir = os.path.join(directory, u'isolate')
      isolate_dir_1 = os.path.join(isolate_dir, u'1')
      isolate_dir_3 = os.path.join(isolate_dir, u'3')
      isolate_dir_3_2 = os.path.join(isolate_dir_3, u'2')
      isolated_dir = os.path.join(directory, u'isolated')
      os.mkdir(isolated_dir)
      os.mkdir(isolate_dir)
      os.mkdir(isolate_dir_1)
      os.mkdir(isolate_dir_3)
      os.mkdir(isolate_dir_3_2)
      isolated = os.path.join(isolated_dir, u'foo.isolated')

      with open(os.path.join(isolate_dir_1, 'isolate1.isolate'), 'wb') as f:
        isolate_format.pretty_print(isolate1, f)
      with open(os.path.join(isolate_dir_3_2, 'isolate2.isolate'), 'wb') as f:
        isolate_format.pretty_print(isolate2, f)
      root_isolate = os.path.join(isolate_dir_3, 'isolate3.isolate')
      with open(root_isolate, 'wb') as f:
        isolate_format.pretty_print(isolate3, f)

      # Make all the touched files.
      path_dir = os.path.join(cwd, 'path')
      os.mkdir(path_dir)
      for v in expected_files:
        f = os.path.join(path_dir, v)
        base = os.path.dirname(f)
        if not os.path.isdir(base):
          os.makedirs(base)
        logging.warn(f)
        open(f, 'wb').close()

      c = isolate.CompleteState(isolated, isolate.SavedState(isolated_dir))
      config = {
        'OS': config_os,
      }
      paths = {
        'PATH': 'path/',
      }
      extra = {
        'EXTRA': 'indeed',
      }
      c.load_isolate(
          unicode(cwd), root_isolate, paths, config, extra, None, False)
      # Note that load_isolate() doesn't retrieve the meta data about each file.
      expected = {
        'algo': 'sha-1',
        'command': command,
        'files': {
          unicode(os.path.join(cwd_name, config_os, 'path', f)): {}
          for f in expected_files
        },
        'read_only': 1,
        'relative_cwd': relative_cwd,
        'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      if not command:
        expected.pop('command')
      self.assertEqual(expected, c.saved_state.to_isolated())

    cwd_name = os.path.basename(self.cwd)
    dir_name = os.path.basename(self.directory)
    test_with_os(
        'amiga',
        (
          'file_amiga',
        ),
        [],
        os.path.join(dir_name, u'amiga', 'isolate', '3'))
    test_with_os(
        'linux',
        (
          u'file_linux',
          os.path.join(u'other', 'file'),
        ),
        [],
        os.path.join(dir_name, u'linux', 'isolate', '3', '2'))
    test_with_os(
        'mac',
        (
          'file_non_linux',
          os.path.join(u'other', 'file'),
          'file_mac',
        ),
        [
          'foo',
          'mac',
          os.path.join(u'..', '..', '..', '..', cwd_name, 'mac', 'path'),
          'indeed',
        ],
        os.path.join(dir_name, u'mac', 'isolate', '3'))
    test_with_os(
        'win',
        (
          'file_non_linux',
        ),
        [],
        os.path.join(dir_name, u'win', 'isolate', '1'))


class IsolateCommand(IsolateBase):
  def load_complete_state(self, *_):
    """Creates a minimalist CompleteState instance without an .isolated
    reference.
    """
    out = isolate.CompleteState(None, isolate.SavedState(self.cwd))
    out.saved_state.isolate_file = u'blah.isolate'
    out.saved_state.relative_cwd = u''
    out.saved_state.root_dir = ROOT_DIR
    return out

  def test_CMDarchive(self):
    actual = []

    def mocked_upload_tree(base_url, infiles, namespace):
      # |infiles| may be a generator of pair, materialize it into a list.
      actual.append({
        'base_url': base_url,
        'infiles': dict(infiles),
        'namespace': namespace,
      })
    self.mock(isolateserver, 'upload_tree', mocked_upload_tree)

    def join(*path):
      return os.path.join(self.cwd, *path)

    isolate_file = join('x.isolate')
    isolated_file = join('x.isolated')
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'files\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(join('foo'), 'wb') as f:
      f.write('fooo')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = [
        '-i', isolate_file,
        '-s', isolated_file,
        '--isolate-server', 'http://localhost:1',
        '--config-variable', 'OS', 'dendy',
    ]
    self.assertEqual(0, isolate.CMDarchive(optparse.OptionParser(), cmd))
    expected = [
        {
          'base_url': 'http://localhost:1',
          'infiles': {
            join(isolated_file): {
              'priority': '0',
            },
            join('foo'): {
              'h': '520d41b29f891bbaccf31d9fcfa72e82ea20fcf0',
              's': 4,
            },
          },
          'namespace': 'default-gzip',
        },
    ]
    # These always change.
    actual[0]['infiles'][join(isolated_file)].pop('h')
    actual[0]['infiles'][join(isolated_file)].pop('s')
    # 'm' is not set on Windows.
    actual[0]['infiles'][join('foo')].pop('m', None)
    actual[0]['infiles'][join('foo')].pop('t')
    self.assertEqual(expected, actual)

  def test_CMDbatcharchive(self):
    # Same as test_CMDarchive but via code path that parses *.gen.json files.
    actual = []

    def mocked_upload_tree(base_url, infiles, namespace):
      # |infiles| may be a generator of pair, materialize it into a list.
      actual.append({
        'base_url': base_url,
        'infiles': dict(infiles),
        'namespace': namespace,
      })
    self.mock(isolateserver, 'upload_tree', mocked_upload_tree)

    def join(*path):
      return os.path.join(self.cwd, *path)

    # First isolate: x.isolate.
    isolate_file_x = join('x.isolate')
    isolated_file_x = join('x.isolated')
    with open(isolate_file_x, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'files\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(join('foo'), 'wb') as f:
      f.write('fooo')
    with open(join('x.isolated.gen.json'), 'wb') as f:
      json.dump({
        'args': [
          '-i', isolate_file_x,
          '-s', isolated_file_x,
          '--config-variable', 'OS', 'dendy',
        ],
        'dir': self.cwd,
        'version': 1,
      }, f)

    # Second isolate: y.isolate.
    isolate_file_y = join('y.isolate')
    isolated_file_y = join('y.isolated')
    with open(isolate_file_y, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'files\': [\'bar\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(join('bar'), 'wb') as f:
      f.write('barr')
    with open(join('y.isolated.gen.json'), 'wb') as f:
      json.dump({
        'args': [
          '-i', isolate_file_y,
          '-s', isolated_file_y,
          '--config-variable', 'OS', 'dendy',
        ],
        'dir': self.cwd,
        'version': 1,
      }, f)

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = [
      '--isolate-server', 'http://localhost:1',
      '--dump-json', 'json_output.json',
      join('x.isolated.gen.json'),
      join('y.isolated.gen.json'),
    ]
    self.assertEqual(
        0,
        isolate.CMDbatcharchive(logging_utils.OptionParserWithLogging(), cmd))
    expected = [
        {
          'base_url': 'http://localhost:1',
          'infiles': {
            join(isolated_file_x): {
              'priority': '0',
            },
            join('foo'): {
              'h': '520d41b29f891bbaccf31d9fcfa72e82ea20fcf0',
              's': 4,
            },
            join(isolated_file_y): {
              'priority': '0',
            },
            join('bar'): {
              'h': 'e918b3a3f9597e3cfdc62ce20ecf5756191cb3ec',
              's': 4,
            },
          },
          'namespace': 'default-gzip',
        },
    ]
    # These always change.
    actual[0]['infiles'][join(isolated_file_x)].pop('h')
    actual[0]['infiles'][join(isolated_file_x)].pop('s')
    actual[0]['infiles'][join('foo')].pop('m', None)
    actual[0]['infiles'][join('foo')].pop('t')
    actual[0]['infiles'][join(isolated_file_y)].pop('h')
    actual[0]['infiles'][join(isolated_file_y)].pop('s')
    actual[0]['infiles'][join('bar')].pop('m', None)
    actual[0]['infiles'][join('bar')].pop('t')
    self.assertEqual(expected, actual)

    expected_json = {
      'x': isolated_format.hash_file(
          os.path.join(self.cwd, 'x.isolated'), ALGO),
      'y': isolated_format.hash_file(
          os.path.join(self.cwd, 'y.isolated'), ALGO),
    }
    self.assertEqual(expected_json, tools.read_json('json_output.json'))

  def test_CMDcheck_empty(self):
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    with open(isolate_file, 'wb') as f:
      f.write('# Foo\n{\n}')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = ['-i', isolate_file, '-s', isolated_file]
    isolate.CMDcheck(optparse.OptionParser(), cmd)

  def test_CMDcheck_stale_version(self):
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'command\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = [
        '-i', isolate_file,
        '-s', isolated_file,
        '--config-variable', 'OS=dendy',
    ]
    self.assertEqual(0, isolate.CMDcheck(optparse.OptionParser(), cmd))

    with open(isolate_file, 'rb') as f:
      actual = f.read()
    expected = (
        '# Foo\n{  \'conditions\':[    [\'OS=="dendy"\', {      '
        '\'variables\': {        \'command\': [\'foo\'],      },    }],  ],}')
    self.assertEqual(expected, actual)

    with open(isolated_file, 'rb') as f:
      actual_isolated = f.read()
    expected_isolated = (
        '{"algo":"sha-1","command":["foo"],"files":{},'
        '"read_only":1,"relative_cwd":".","version":"%s"}'
    ) % isolated_format.ISOLATED_FILE_VERSION
    self.assertEqual(expected_isolated, actual_isolated)
    isolated_data = json.loads(actual_isolated)

    with open(isolated_file + '.state', 'rb') as f:
      actual_isolated_state = f.read()
    expected_isolated_state = (
        '{"OS":"%s","algo":"sha-1","child_isolated_files":[],"command":["foo"],'
        '"config_variables":{"OS":"dendy"},'
        '"extra_variables":{"EXECUTABLE_SUFFIX":"%s"},"files":{},'
        '"isolate_file":"x.isolate","path_variables":{},'
        '"relative_cwd":".","root_dir":%s,"version":"%s"}'
    ) % (
      sys.platform,
      '.exe' if sys.platform=='win32' else '',
      json.dumps(self.cwd),
      isolate.SavedState.EXPECTED_VERSION)
    self.assertEqual(expected_isolated_state, actual_isolated_state)
    isolated_state_data = json.loads(actual_isolated_state)

    # Now edit the .isolated.state file to break the version number and make
    # sure it doesn't crash.
    with open(isolated_file + '.state', 'wb') as f:
      isolated_state_data['version'] = '100.42'
      json.dump(isolated_state_data, f)
    self.assertEqual(0, isolate.CMDcheck(optparse.OptionParser(), cmd))

    # Now edit the .isolated file to break the version number and make
    # sure it doesn't crash.
    with open(isolated_file, 'wb') as f:
      isolated_data['version'] = '100.42'
      json.dump(isolated_data, f)
    self.assertEqual(0, isolate.CMDcheck(optparse.OptionParser(), cmd))

    # Make sure the files were regenerated.
    with open(isolated_file, 'rb') as f:
      actual_isolated = f.read()
    self.assertEqual(expected_isolated, actual_isolated)
    with open(isolated_file + '.state', 'rb') as f:
      actual_isolated_state = f.read()
    self.assertEqual(expected_isolated_state, actual_isolated_state)

  def test_CMDcheck_new_variables(self):
    # Test bug #61.
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    cmd = [
        '-i', isolate_file,
        '-s', isolated_file,
        '--config-variable', 'OS=dendy',
    ]
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'command\': [\'foo\'],'
          '        \'files\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(os.path.join(self.cwd, 'foo'), 'wb') as f:
      f.write('yeah')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    self.assertEqual(0, isolate.CMDcheck(optparse.OptionParser(), cmd))

    # Now add a new config variable.
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'command\': [\'foo\'],'
          '        \'files\': [\'foo\'],'
          '      },'
          '    }],'
          '    [\'foo=="baz"\', {'
          '      \'variables\': {'
          '        \'files\': [\'bar\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(os.path.join(self.cwd, 'bar'), 'wb') as f:
      f.write('yeah right!')

    # The configuration is OS=dendy and foo=bar. So it should load both
    # configurations.
    self.assertEqual(
        0,
        isolate.CMDcheck(
            optparse.OptionParser(), cmd + ['--config-variable', 'foo=bar']))

  def test_CMDcheck_isolate_copied(self):
    # Note that moving the .isolate file is a different code path, this is about
    # copying the .isolate file to a new place and specifying the new location
    # on a subsequent execution.
    x_isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    cmd = ['-i', x_isolate_file, '-s', isolated_file]
    with open(x_isolate_file, 'wb') as f:
      f.write('{}')
    self.assertEqual(0, isolate.CMDcheck(optparse.OptionParser(), cmd))
    self.assertTrue(os.path.isfile(isolated_file + '.state'))
    with open(isolated_file + '.state', 'rb') as f:
      self.assertEqual(json.load(f)['isolate_file'], 'x.isolate')

    # Move the .isolate file.
    y_isolate_file = os.path.join(self.cwd, 'Y.isolate')
    shutil.copyfile(x_isolate_file, y_isolate_file)
    cmd = ['-i', y_isolate_file, '-s', isolated_file]
    self.assertEqual(0, isolate.CMDcheck(optparse.OptionParser(), cmd))
    with open(isolated_file + '.state', 'rb') as f:
      self.assertEqual(json.load(f)['isolate_file'], 'Y.isolate')

  def test_CMDrun_extra_args(self):
    cmd = [
      'run',
      '--isolate', 'blah.isolate',
      '--', 'extra_args',
    ]
    self.mock(isolate, 'load_complete_state', self.load_complete_state)
    self.mock(subprocess, 'call', lambda *_, **_kwargs: 0)
    self.assertEqual(0, isolate.CMDrun(optparse.OptionParser(), cmd))

  def test_CMDrun_no_isolated(self):
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    with open(isolate_file, 'wb') as f:
      f.write('{"variables": {"command": ["python", "-c", "print(\'hi\')"]} }')

    def expect_call(cmd, cwd):
      self.assertEqual([sys.executable, '-c', "print('hi')", 'run'], cmd)
      self.assertTrue(os.path.isdir(cwd))
      return 0
    self.mock(subprocess, 'call', expect_call)

    cmd = ['run', '--isolate', isolate_file]
    self.assertEqual(0, isolate.CMDrun(optparse.OptionParser(), cmd))


def clear_env_vars():
  for e in ('ISOLATE_DEBUG', 'ISOLATE_SERVER'):
    os.environ.pop(e, None)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  clear_env_vars()
  test_utils.main()
