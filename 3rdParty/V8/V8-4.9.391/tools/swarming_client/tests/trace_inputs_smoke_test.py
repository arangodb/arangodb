#!/usr/bin/env python
# coding=utf-8
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import functools
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unicodedata
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import trace_inputs
from third_party.depot_tools import fix_encoding
from utils import file_path
from utils import threading_utils


FILENAME = os.path.basename(__file__)
REL_DATA = os.path.join(u'tests', 'trace_inputs')
VERBOSE = False

# TODO(maruel): Have the kernel tracer on Windows differentiate between file
# read or file write.
MODE_R = trace_inputs.Results.File.READ if sys.platform != 'win32' else None
MODE_W = trace_inputs.Results.File.WRITE if sys.platform != 'win32' else None
MODE_T = trace_inputs.Results.File.TOUCHED


def check_can_trace(fn):
  """Function decorator that skips test that need to be able trace."""
  @functools.wraps(fn)
  def hook(self, *args, **kwargs):
    if not trace_inputs.can_trace():
      self.fail('Please rerun this test with admin privileges.')
    return fn(self, *args, **kwargs)
  return hook


class CalledProcessError(subprocess.CalledProcessError):
  """Makes 2.6 version act like 2.7"""
  def __init__(self, returncode, cmd, output, cwd):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output
    self.cwd = cwd

  def __str__(self):
    return super(CalledProcessError, self).__str__() + (
        '\n'
        'cwd=%s\n%s') % (self.cwd, self.output)


class TraceInputsBase(unittest.TestCase):
  def setUp(self):
    self.tempdir = None
    self.trace_inputs_path = os.path.join(ROOT_DIR, 'trace_inputs.py')

    # Wraps up all the differences between OSes here.
    # - Windows doesn't track initial_cwd.
    # - OSX replaces /usr/bin/python with /usr/bin/python2.7.
    self.cwd = os.path.join(ROOT_DIR, u'tests')
    self.initial_cwd = unicode(self.cwd)
    self.expected_cwd = unicode(ROOT_DIR)
    if sys.platform == 'win32':
      # Not supported on Windows.
      self.initial_cwd = None
      self.expected_cwd = None

    # There's 3 kinds of references to python, self.executable,
    # self.real_executable and self.naked_executable. It depends how python was
    # started.
    self.executable = sys.executable
    if sys.platform == 'darwin':
      # /usr/bin/python is a thunk executable that decides which version of
      # python gets executed.
      suffix = '.'.join(map(str, sys.version_info[0:2]))
      if os.access(self.executable + suffix, os.X_OK):
        # So it'll look like /usr/bin/python2.7
        self.executable += suffix

    self.real_executable = file_path.get_native_path_case(
        unicode(self.executable))
    self.tempdir = file_path.get_native_path_case(
        unicode(tempfile.mkdtemp(prefix=u'trace_smoke_test')))
    self.log = os.path.join(self.tempdir, 'log')

    # self.naked_executable will only be naked on Windows.
    self.naked_executable = unicode(sys.executable)
    if sys.platform == 'win32':
      self.naked_executable = os.path.basename(sys.executable)

  def tearDown(self):
    if self.tempdir:
      if VERBOSE:
        print 'Leaking: %s' % self.tempdir
      else:
        file_path.rmtree(self.tempdir)

  @staticmethod
  def get_child_command(from_data):
    """Returns command to run the child1.py."""
    cmd = [sys.executable]
    if from_data:
      # When the gyp argument is specified, the command is started from --cwd
      # directory. In this case, 'tests'.
      cmd.extend([os.path.join('trace_inputs', 'child1.py'), '--child-gyp'])
    else:
      # When the gyp argument is not specified, the command is started from
      # --root-dir directory.
      cmd.extend([os.path.join(REL_DATA, 'child1.py'), '--child'])
    return cmd

  @staticmethod
  def _size(*args):
    return os.stat(os.path.join(ROOT_DIR, *args)).st_size


class TraceInputs(TraceInputsBase):
  def _execute(self, mode, command, cwd):
    cmd = [
      sys.executable,
      self.trace_inputs_path,
      mode,
      '--log', self.log,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    cmd.extend(command)
    logging.info('Command: %s' % ' '.join(cmd))
    p = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=cwd,
        universal_newlines=True)
    out, err = p.communicate()
    if VERBOSE:
      print err
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out + err, cwd)
    return out or ''

  def _trace(self, from_data):
    if from_data:
      cwd = os.path.join(ROOT_DIR, 'tests')
    else:
      cwd = ROOT_DIR
    return self._execute('trace', self.get_child_command(from_data), cwd=cwd)

  @check_can_trace
  def test_trace(self):
    expected = '\n'.join((
      'Total: 7',
      'Non existent: 0',
      'Interesting: 7 reduced to 6',
      '  tests/trace_inputs/child1.py'.replace('/', os.path.sep),
      '  tests/trace_inputs/child2.py'.replace('/', os.path.sep),
      '  tests/trace_inputs/files1/'.replace('/', os.path.sep),
      '  tests/trace_inputs/test_file.txt'.replace('/', os.path.sep),
      ('  tests/%s' % FILENAME).replace('/', os.path.sep),
      '  trace_inputs.py',
    )) + '\n'
    trace_expected = '\n'.join((
      'child from %s' % ROOT_DIR,
      'child2',
    )) + '\n'
    trace_actual = self._trace(False)
    actual = self._execute(
        'read',
        [
          '--root-dir', ROOT_DIR,
          '--trace-blacklist', '.+\\.pyc',
          '--trace-blacklist', '.*\\.svn',
          '--trace-blacklist', '.*do_not_care\\.txt',
        ],
        cwd=unicode(ROOT_DIR))
    self.assertEqual(expected, actual)
    self.assertEqual(trace_expected, trace_actual)

  @check_can_trace
  def test_trace_json(self):
    expected = {
      u'root': {
        u'children': [
          {
            u'children': [],
            u'command': [u'python', u'child2.py'],
            u'executable': self.naked_executable,
            u'files': [
              {
                'mode': MODE_R,
                u'path': os.path.join(REL_DATA, 'child2.py'),
                u'size': self._size(REL_DATA, 'child2.py'),
              },
              {
                'mode': MODE_R,
                u'path': os.path.join(REL_DATA, 'files1', 'bar'),
                u'size': self._size(REL_DATA, 'files1', 'bar'),
              },
              {
                'mode': MODE_R,
                u'path': os.path.join(REL_DATA, 'files1', 'foo'),
                u'size': self._size(REL_DATA, 'files1', 'foo'),
              },
              {
                'mode': MODE_R,
                u'path': os.path.join(REL_DATA, 'test_file.txt'),
                u'size': self._size(REL_DATA, 'test_file.txt'),
              },
            ],
            u'initial_cwd': self.initial_cwd,
            #u'pid': 123,
          },
        ],
        u'command': [
          unicode(self.executable),
          os.path.join(u'trace_inputs', 'child1.py'),
          u'--child-gyp',
        ],
        u'executable': self.real_executable,
        u'files': [
          {
            u'mode': MODE_R,
            u'path': os.path.join(REL_DATA, 'child1.py'),
            u'size': self._size(REL_DATA, 'child1.py'),
          },
          {
            u'mode': MODE_R,
            u'path': os.path.join(u'tests', u'trace_inputs_smoke_test.py'),
            u'size': self._size('tests', 'trace_inputs_smoke_test.py'),
          },
          {
            u'mode': MODE_R,
            u'path': u'trace_inputs.py',
            u'size': self._size('trace_inputs.py'),
          },
        ],
        u'initial_cwd': self.initial_cwd,
        #u'pid': 123,
      },
    }
    trace_expected = '\n'.join((
      'child_gyp from %s' % os.path.join(ROOT_DIR, 'tests'),
      'child2',
    )) + '\n'
    trace_actual = self._trace(True)
    actual_text = self._execute(
        'read',
        [
          '--root-dir', ROOT_DIR,
          '--trace-blacklist', '.+\\.pyc',
          '--trace-blacklist', '.*\\.svn',
          '--trace-blacklist', '.*do_not_care\\.txt',
          '--json',
        ],
        cwd=unicode(ROOT_DIR))
    actual_json = json.loads(actual_text)
    self.assertEqual(list, actual_json.__class__)
    self.assertEqual(1, len(actual_json))
    actual_json = actual_json[0]
    # Removes the pids.
    self.assertTrue(actual_json['root'].pop('pid'))
    self.assertTrue(actual_json['root']['children'][0].pop('pid'))
    self.assertEqual(expected, actual_json)
    self.assertEqual(trace_expected, trace_actual)


class TraceInputsImport(TraceInputsBase):

  # Similar to TraceInputs test fixture except that it calls the function
  # directly, so the Results instance can be inspected.
  # Roughly, make sure the API is stable.
  def _execute_trace(self, command):
    # Similar to what trace_test_cases.py does.
    api = trace_inputs.get_api()
    _, _ = trace_inputs.trace(self.log, command, self.cwd, api, True)
    # TODO(maruel): Check
    #self.assertEqual(0, returncode)
    #self.assertEqual('', output)
    def blacklist(f):
      return f.endswith(('.pyc', '.svn', 'do_not_care.txt'))
    data = api.parse_log(self.log, blacklist, None)
    self.assertEqual(1, len(data))
    if 'exception' in data[0]:
      raise data[0]['exception'][0], \
          data[0]['exception'][1], \
          data[0]['exception'][2]

    return data[0]['results'].strip_root(unicode(ROOT_DIR))

  def _gen_dict_wrong_path(self):
    """Returns the expected flattened Results when child1.py is called with the
    wrong relative path.
    """
    return {
      'root': {
        'children': [],
        'command': [
          self.executable,
          os.path.join(REL_DATA, 'child1.py'),
          '--child',
        ],
        'executable': self.real_executable,
        'files': [],
        'initial_cwd': self.initial_cwd,
      },
    }

  def _gen_dict_full(self):
    """Returns the expected flattened Results when child1.py is called with
    --child.
    """
    return {
      'root': {
        'children': [
          {
            'children': [],
            'command': ['python', 'child2.py'],
            'executable': self.naked_executable,
            'files': [
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'child2.py'),
                'size': self._size(REL_DATA, 'child2.py'),
              },
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'files1', 'bar'),
                'size': self._size(REL_DATA, 'files1', 'bar'),
              },
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'files1', 'foo'),
                'size': self._size(REL_DATA, 'files1', 'foo'),
              },
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'test_file.txt'),
                'size': self._size(REL_DATA, 'test_file.txt'),
              },
            ],
            'initial_cwd': self.expected_cwd,
          },
        ],
        'command': [
          self.executable,
          os.path.join(REL_DATA, 'child1.py'),
          '--child',
        ],
        'executable': self.real_executable,
        'files': [
          {
            'mode': MODE_R,
            'path': os.path.join(REL_DATA, 'child1.py'),
            'size': self._size(REL_DATA, 'child1.py'),
          },
          {
            'mode': MODE_R,
            'path': os.path.join(u'tests', u'trace_inputs_smoke_test.py'),
            'size': self._size('tests', 'trace_inputs_smoke_test.py'),
          },
          {
            'mode': MODE_R,
            'path': u'trace_inputs.py',
            'size': self._size('trace_inputs.py'),
          },
        ],
        'initial_cwd': self.expected_cwd,
      },
    }

  def _gen_dict_full_gyp(self):
    """Returns the expected flattened results when child1.py is called with
    --child-gyp.
    """
    return {
      'root': {
        'children': [
          {
            'children': [],
            'command': [u'python', u'child2.py'],
            'executable': self.naked_executable,
            'files': [
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'child2.py'),
                'size': self._size(REL_DATA, 'child2.py'),
              },
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'files1', 'bar'),
                'size': self._size(REL_DATA, 'files1', 'bar'),
              },
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'files1', 'foo'),
                'size': self._size(REL_DATA, 'files1', 'foo'),
              },
              {
                'mode': MODE_R,
                'path': os.path.join(REL_DATA, 'test_file.txt'),
                'size': self._size(REL_DATA, 'test_file.txt'),
              },
            ],
            'initial_cwd': self.initial_cwd,
          },
        ],
        'command': [
          self.executable,
          os.path.join('trace_inputs', 'child1.py'),
          '--child-gyp',
        ],
        'executable': self.real_executable,
        'files': [
          {
            'mode': MODE_R,
            'path': os.path.join(REL_DATA, 'child1.py'),
            'size': self._size(REL_DATA, 'child1.py'),
          },
          {
            'mode': MODE_R,
            'path': os.path.join(u'tests', u'trace_inputs_smoke_test.py'),
            'size': self._size('tests', 'trace_inputs_smoke_test.py'),
          },
          {
            'mode': MODE_R,
            'path': u'trace_inputs.py',
            'size': self._size('trace_inputs.py'),
          },
        ],
        'initial_cwd': self.initial_cwd,
      },
    }

  @check_can_trace
  def test_trace_wrong_path(self):
    # Deliberately start the trace from the wrong path. Starts it from the
    # directory 'tests' so 'tests/tests/trace_inputs/child1.py' is not
    # accessible, so child2.py process is not started.
    results = self._execute_trace(self.get_child_command(False))
    expected = self._gen_dict_wrong_path()
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEqual(expected, actual)

  @check_can_trace
  def test_trace(self):
    expected = self._gen_dict_full_gyp()
    results = self._execute_trace(self.get_child_command(True))
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertTrue(actual['root']['children'][0].pop('pid'))
    self.assertEqual(expected, actual)
    files = [
      u'tests/trace_inputs/child1.py'.replace('/', os.path.sep),
      u'tests/trace_inputs/child2.py'.replace('/', os.path.sep),
      u'tests/trace_inputs/files1/'.replace('/', os.path.sep),
      u'tests/trace_inputs/test_file.txt'.replace('/', os.path.sep),
      u'tests/trace_inputs_smoke_test.py'.replace('/', os.path.sep),
      u'trace_inputs.py',
    ]
    def blacklist(f):
      return f.endswith(('.pyc', 'do_not_care.txt', '.git', '.svn'))
    simplified = trace_inputs.extract_directories(
        file_path.get_native_path_case(unicode(ROOT_DIR)),
        results.files,
        blacklist)
    self.assertEqual(files, [f.path for f in simplified])

  @check_can_trace
  def test_trace_multiple(self):
    # Starts parallel threads and trace parallel child processes simultaneously.
    # Some are started from 'tests' directory, others from this script's
    # directory. One trace fails. Verify everything still goes one.
    parallel = 8

    def trace(tracer, cmd, cwd, tracename):
      resultcode, output = tracer.trace(cmd, cwd, tracename, True)
      return (tracename, resultcode, output)

    with threading_utils.ThreadPool(parallel, parallel, 0) as pool:
      api = trace_inputs.get_api()
      with api.get_tracer(self.log) as tracer:
        pool.add_task(
            0, trace, tracer, self.get_child_command(False), ROOT_DIR, 'trace1')
        pool.add_task(
            0, trace, tracer, self.get_child_command(True), self.cwd, 'trace2')
        pool.add_task(
            0, trace, tracer, self.get_child_command(False), ROOT_DIR, 'trace3')
        pool.add_task(
            0, trace, tracer, self.get_child_command(True), self.cwd, 'trace4')
        # Have this one fail since it's started from the wrong directory.
        pool.add_task(
            0, trace, tracer, self.get_child_command(False), self.cwd, 'trace5')
        pool.add_task(
            0, trace, tracer, self.get_child_command(True), self.cwd, 'trace6')
        pool.add_task(
            0, trace, tracer, self.get_child_command(False), ROOT_DIR, 'trace7')
        pool.add_task(
            0, trace, tracer, self.get_child_command(True), self.cwd, 'trace8')
        trace_results = pool.join()
    def blacklist(f):
      return f.endswith(('.pyc', 'do_not_care.txt', '.git', '.svn'))
    actual_results = api.parse_log(self.log, blacklist, None)
    self.assertEqual(8, len(trace_results))
    self.assertEqual(8, len(actual_results))

    # Convert to dict keyed on the trace name, simpler to verify.
    trace_results = dict((i[0], i[1:]) for i in trace_results)
    actual_results = dict((x.pop('trace'), x) for x in actual_results)
    self.assertEqual(sorted(trace_results), sorted(actual_results))

    # It'd be nice to start different kinds of processes.
    expected_results = [
      self._gen_dict_full(),
      self._gen_dict_full_gyp(),
      self._gen_dict_full(),
      self._gen_dict_full_gyp(),
      self._gen_dict_wrong_path(),
      self._gen_dict_full_gyp(),
      self._gen_dict_full(),
      self._gen_dict_full_gyp(),
    ]
    self.assertEqual(len(expected_results), len(trace_results))

    # See the comment above about the trace that fails because it's started from
    # the wrong directory.
    busted = 4
    for index, key in enumerate(sorted(actual_results)):
      self.assertEqual('trace%d' % (index + 1), key)
      self.assertEqual(2, len(trace_results[key]))
      # returncode
      self.assertEqual(0 if index != busted else 2, trace_results[key][0])
      # output
      self.assertEqual(actual_results[key]['output'], trace_results[key][1])

      self.assertEqual(['output', 'results'], sorted(actual_results[key]))
      results = actual_results[key]['results']
      results = results.strip_root(unicode(ROOT_DIR))
      actual = results.flatten()
      self.assertTrue(actual['root'].pop('pid'))
      if index != busted:
        self.assertTrue(actual['root']['children'][0].pop('pid'))
      self.assertEqual(expected_results[index], actual)

  if sys.platform != 'win32':
    def test_trace_symlink(self):
      expected = {
        'root': {
          'children': [],
          'command': [
            self.executable,
            os.path.join('trace_inputs', 'symlink.py'),
          ],
          'executable': self.real_executable,
          'files': [
            {
              'mode': MODE_R,
              'path': os.path.join(REL_DATA, 'files2', 'bar'),
              'size': self._size(REL_DATA, 'files2', 'bar'),
            },
            {
              'mode': MODE_R,
              'path': os.path.join(REL_DATA, 'files2', 'foo'),
              'size': self._size(REL_DATA, 'files2', 'foo'),
            },
            {
              'mode': MODE_R,
              'path': os.path.join(REL_DATA, 'symlink.py'),
              'size': self._size(REL_DATA, 'symlink.py'),
            },
          ],
          'initial_cwd': self.initial_cwd,
        },
      }
      cmd = [sys.executable, os.path.join('trace_inputs', 'symlink.py')]
      results = self._execute_trace(cmd)
      actual = results.flatten()
      self.assertTrue(actual['root'].pop('pid'))
      self.assertEqual(expected, actual)
      files = [
        # In particular, the symlink is *not* resolved.
        u'tests/trace_inputs/files2/'.replace('/', os.path.sep),
        u'tests/trace_inputs/symlink.py'.replace('/', os.path.sep),
      ]
      def blacklist(f):
        return f.endswith(('.pyc', '.svn', 'do_not_care.txt'))
      simplified = trace_inputs.extract_directories(
          unicode(ROOT_DIR), results.files, blacklist)
      self.assertEqual(files, [f.path for f in simplified])

  @check_can_trace
  def test_trace_quoted(self):
    results = self._execute_trace([sys.executable, '-c', 'print("hi")'])
    expected = {
      'root': {
        'children': [],
        'command': [
          self.executable,
          '-c',
          'print("hi")',
        ],
        'executable': self.real_executable,
        'files': [],
        'initial_cwd': self.initial_cwd,
      },
    }
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEqual(expected, actual)

  @check_can_trace
  def _touch_expected(self, command):
    # Looks for file that were touched but not opened, using different apis.
    results = self._execute_trace(
      [sys.executable, os.path.join('trace_inputs', 'touch_only.py'), command])
    expected = {
      'root': {
        'children': [],
        'command': [
          self.executable,
          os.path.join('trace_inputs', 'touch_only.py'),
          command,
        ],
        'executable': self.real_executable,
        'files': [
          {
            'mode': MODE_T,
            'path': os.path.join(REL_DATA, 'test_file.txt'),
            'size': self._size(REL_DATA, 'test_file.txt'),
          },
          {
            'mode': MODE_R,
            'path': os.path.join(REL_DATA, 'touch_only.py'),
            'size': self._size(REL_DATA, 'touch_only.py'),
          },
        ],
        'initial_cwd': self.initial_cwd,
      },
    }
    if sys.platform != 'linux2':
      # TODO(maruel): Remove once properly implemented.
      expected['root']['files'].pop(0)

    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEqual(expected, actual)

  def test_trace_touch_only_access(self):
    self._touch_expected('access')

  def test_trace_touch_only_isfile(self):
    self._touch_expected('isfile')

  def test_trace_touch_only_stat(self):
    self._touch_expected('stat')

  @check_can_trace
  def test_trace_tricky_filename(self):
    # TODO(maruel):  On Windows, it's using the current code page so some
    # characters can't be represented. As a nice North American, hard code the
    # string to something representable in code page 1252. The exact code page
    # depends on the user system.
    if sys.platform == 'win32':
      filename = u'foo, bar,  ~p#o,,ué^t%t .txt'
    else:
      filename = u'foo, bar,  ~p#o,,ué^t%t 和平.txt'

    exe = os.path.join(self.tempdir, 'tricky_filename.py')
    shutil.copyfile(
        os.path.join(self.cwd, 'trace_inputs', 'tricky_filename.py'), exe)
    expected = {
      'root': {
        'children': [],
        'command': [
          self.executable,
          exe,
        ],
        'executable': self.real_executable,
        'files': [
          {
            'mode': MODE_W,
            'path':  filename,
            'size': long(len('Bingo!')),
          },
          {
            'mode': MODE_R,
            'path': u'tricky_filename.py',
            'size': self._size(REL_DATA, 'tricky_filename.py'),
          },
        ],
        'initial_cwd': self.tempdir if sys.platform != 'win32' else None,
      },
    }

    api = trace_inputs.get_api()
    returncode, output = trace_inputs.trace(
        self.log, [exe], self.tempdir, api, True)
    self.assertEqual('', output)
    self.assertEqual(0, returncode)
    data = api.parse_log(self.log, lambda _: False, None)
    self.assertEqual(1, len(data))
    if 'exception' in data[0]:
      raise data[0]['exception'][0], \
          data[0]['exception'][1], \
          data[0]['exception'][2]
    actual = data[0]['results'].strip_root(self.tempdir).flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEqual(expected, actual)
    trace_inputs.get_api().clean_trace(self.log)
    files = sorted(
        unicodedata.normalize('NFC', i)
        for i in os.listdir(unicode(self.tempdir)))
    self.assertEqual([filename, 'tricky_filename.py'], files)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  if VERBOSE:
    unittest.TestCase.maxDiff = None
  # Necessary for the dtrace logger to work around execve() hook. See
  # trace_inputs.py for more details.
  os.environ['TRACE_INPUTS_DTRACE_ENABLE_EXECVE'] = '1'
  print >> sys.stderr, 'Test are currently disabled'
  sys.exit(0)
  #unittest.main()
