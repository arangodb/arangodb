#!/usr/bin/env python
# coding=utf-8
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import StringIO
import logging
import os
import sys
import unittest

BASE_DIR = unicode(os.path.dirname(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

FILE_PATH = unicode(os.path.abspath(__file__))

import trace_inputs

# Access to a protected member _FOO of a client class
# pylint: disable=W0212


def join_norm(*args):
  """Joins and normalizes path in a single step."""
  return unicode(os.path.normpath(os.path.join(*args)))


class TraceInputs(unittest.TestCase):
  def test_process_quoted_arguments(self):
    test_cases = (
      ('"foo"', ['foo']),
      ('"foo", "bar"', ['foo', 'bar']),
      ('"foo"..., "bar"', ['foo', 'bar']),
      ('"foo", "bar"...', ['foo', 'bar']),
      (
        '"/browser_tests", "--type=use,comma"',
        ['/browser_tests', '--type=use,comma']
      ),
      (
        '"/browser_tests", "--ignored=\\" --type=renderer \\""',
        ['/browser_tests', '--ignored=" --type=renderer "']
      ),
      (
        '"/Release+Asserts/bin/clang", "-cc1", ...',
        ['/Release+Asserts/bin/clang', '-cc1'],
      ),
    )
    for actual, expected in test_cases:
      self.assertEqual(
          expected, trace_inputs.strace_process_quoted_arguments(actual))

  def test_process_escaped_arguments(self):
    test_cases = (
      ('foo\\0', ['foo']),
      ('foo\\001bar\\0', ['foo', 'bar']),
      ('\\"foo\\"\\0', ['"foo"']),
    )
    for actual, expected in test_cases:
      self.assertEqual(
          expected,
          trace_inputs.Dtrace.Context.process_escaped_arguments(actual))

  def test_variable_abs(self):
    value = trace_inputs.Results.File(
        None, u'/foo/bar', None, None, trace_inputs.Results.File.READ)
    actual = value.replace_variables({'$FOO': '/foo'})
    self.assertEqual('$FOO/bar', actual.path)
    self.assertEqual('$FOO/bar', actual.full_path)
    self.assertEqual(True, actual.tainted)

  def test_variable_rel(self):
    value = trace_inputs.Results.File(
        u'/usr', u'foo/bar', None, None, trace_inputs.Results.File.READ)
    actual = value.replace_variables({'$FOO': 'foo'})
    self.assertEqual('$FOO/bar', actual.path)
    self.assertEqual(os.path.join('/usr', '$FOO/bar'), actual.full_path)
    self.assertEqual(True, actual.tainted)

  def test_strace_filename(self):
    filename = u'foo, bar,  ~p#o,,ué^t%t.txt'
    data = 'foo, bar,  ~p#o,,u\\303\\251^t%t.txt'
    self.assertEqual(filename, trace_inputs.Strace.load_filename(data))

  def test_CsvReader(self):
    test_cases = {
      u'   Next is empty, ,  {00000000-0000}':
        [u'Next is empty', u'', u'{00000000-0000}'],

      u'   Foo, , "\\\\NT AUTHORITY\\SYSTEM", "Idle", ""':
        [u'Foo', u'', u'\\\\NT AUTHORITY\\SYSTEM', u'Idle', u''],

      u'   Foo,  ""Who the hell thought delimiters are great as escape too""':
        [u'Foo', u'"Who the hell thought delimiters are great as escape too"'],

      (
        u'  "remoting.exe", ""C:\\Program Files\\remoting.exe" '
        u'--host="C:\\ProgramData\\host.json""'
      ):
        [
          u'remoting.exe',
          u'"C:\\Program Files\\remoting.exe" '
          u'--host="C:\\ProgramData\\host.json"'
        ],

      u'"MONSTRE", "", 0x0': [u'MONSTRE', u'', u'0x0'],

      # To whoever wrote this code at Microsoft: You did it wrong.
      u'"cmd.exe", ""C:\\\\Winz\\\\cmd.exe" /k ""C:\\\\MSVS\\\\vc.bat"" x86"':
        [u'cmd.exe', u'"C:\\\\Winz\\\\cmd.exe" /k "C:\\\\MSVS\\\\vc.bat" x86'],
    }
    for data, expected in test_cases.iteritems():
      csv = trace_inputs.LogmanTrace.Tracer.CsvReader(StringIO.StringIO(data))
      actual = [i for i in csv]
      self.assertEqual(1, len(actual))
      self.assertEqual(expected, actual[0])


if sys.platform != 'win32':
  class StraceInputs(unittest.TestCase):
    # Represents the root process pid (an arbitrary number).
    _ROOT_PID = 27
    _CHILD_PID = 14
    _GRAND_CHILD_PID = 70

    @staticmethod
    def _load_context(lines, initial_cwd):
      context = trace_inputs.Strace.Context(lambda _: False, None, initial_cwd)
      for line in lines:
        context.on_line(*line)
      done = any(p._done for p in context._process_lookup.itervalues())
      return context.to_results().flatten(), done

    def assertContext(self, lines, initial_cwd, expected, expected_done):
      actual, actual_done = self._load_context(lines, initial_cwd)
      self.assertEqual(expected, actual)
      # If actual_done is True, this means the log was cut off abruptly.
      self.assertEqual(expected_done, actual_done)

    def _test_lines(self, lines, initial_cwd, files, command=None):
      filepath = join_norm(initial_cwd, '../out/unittests')
      command = command or ['../out/unittests']
      expected = {
        'root': {
          'children': [],
          'command': command,
          'executable': filepath,
          'files': files,
          'initial_cwd': initial_cwd,
          'pid': self._ROOT_PID,
        }
      }
      if not files:
        expected['root']['command'] = None
        expected['root']['executable'] = None
      self.assertContext(lines, initial_cwd, expected, False)

    def test_execve(self):
      lines = [
        (self._ROOT_PID,
          'execve("/home/foo_bar_user/out/unittests", '
            '["/home/foo_bar_user/out/unittests", '
            '"--random-flag"], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 8'),
      ]
      files = [
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'mode': trace_inputs.Results.File.WRITE,
          'path': u'/home/foo_bar_user/src/out/unittests.log',
          'size': -1,
        },
      ]
      command = [
        '/home/foo_bar_user/out/unittests', '--random-flag',
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', files, command)

    def test_empty(self):
      try:
        self._load_context([], None)
        self.fail()
      except trace_inputs.TracingFailure, e:
        expected = (
          'Found internal inconsitency in process lifetime detection '
          'while finding the root process',
          None,
          None,
          None,
          None,
          [])
        self.assertEqual(expected, e.args)

    def test_chmod(self):
      lines = [
          (self._ROOT_PID, 'chmod("temp/file", 0100644) = 0'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [
            {
              'mode': trace_inputs.Results.File.WRITE,
              'path': u'/home/foo_bar_user/src/temp/file',
              'size': -1,
            },
          ],
          'initial_cwd': u'/home/foo_bar_user/src',
          'pid': self._ROOT_PID,
        }
      }
      self.assertContext(lines, u'/home/foo_bar_user/src', expected, False)

    def test_close(self):
      lines = [
        (self._ROOT_PID, 'close(7)                          = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_clone(self):
      # Grand-child with relative directory.
      lines = [
        (self._ROOT_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._CHILD_PID),
        (self._CHILD_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._GRAND_CHILD_PID),
        (self._GRAND_CHILD_PID,
          'open("%s", O_RDONLY)       = 76' % os.path.basename(str(FILE_PATH))),
      ]
      size = os.stat(FILE_PATH).st_size
      expected = {
        'root': {
          'children': [
            {
              'children': [
                {
                  'children': [],
                  'command': None,
                  'executable': None,
                  'files': [
                    {
                      'mode': trace_inputs.Results.File.READ,
                      'path': FILE_PATH,
                      'size': size,
                    },
                  ],
                  'initial_cwd': BASE_DIR,
                  'pid': self._GRAND_CHILD_PID,
                },
              ],
              'command': None,
              'executable': None,
              'files': [],
              'initial_cwd': BASE_DIR,
              'pid': self._CHILD_PID,
            },
          ],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': BASE_DIR,
          'pid': self._ROOT_PID,
        },
      }
      self.assertContext(lines, BASE_DIR, expected, False)

    def test_clone_chdir(self):
      # Grand-child with relative directory.
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._CHILD_PID),
        (self._CHILD_PID,
          'chdir("/home_foo_bar_user/path1") = 0'),
        (self._CHILD_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._GRAND_CHILD_PID),
        (self._GRAND_CHILD_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID, 'chdir("/home_foo_bar_user/path2") = 0'),
        (self._GRAND_CHILD_PID,
          'open("random.txt", O_RDONLY)       = 76'),
      ]
      expected = {
        'root': {
          'children': [
            {
              'children': [
                {
                  'children': [],
                  'command': ['../out/unittests'],
                  'executable': '/home_foo_bar_user/out/unittests',
                  'files': [
                    {
                      'mode': trace_inputs.Results.File.READ,
                      'path': u'/home_foo_bar_user/out/unittests',
                      'size': -1,
                    },
                    {
                      'mode': trace_inputs.Results.File.READ,
                      'path': u'/home_foo_bar_user/path1/random.txt',
                      'size': -1,
                    },
                  ],
                  'initial_cwd': u'/home_foo_bar_user/path1',
                  'pid': self._GRAND_CHILD_PID,
                },
              ],
              # clone does not carry over the command and executable so it is
              # clear if an execve() call was done or not.
              'command': None,
              'executable': None,
              # This is important, since no execve call was done, it didn't
              # touch the executable file.
              'files': [],
              'initial_cwd': unicode(ROOT_DIR),
              'pid': self._CHILD_PID,
            },
          ],
          'command': ['../out/unittests'],
          'executable': join_norm(ROOT_DIR, '../out/unittests'),
          'files': [
            {
              'mode': trace_inputs.Results.File.READ,
              'path': join_norm(ROOT_DIR, '../out/unittests'),
              'size': -1,
            },
          ],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertContext(lines, ROOT_DIR, expected, False)

    def test_faccess(self):
      lines = [
        (self._ROOT_PID,
         'faccessat(AT_FDCWD, "/home_foo_bar_user/file", W_OK) = 0'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [
            {
              'mode': trace_inputs.Results.File.TOUCHED,
              'path': u'/home_foo_bar_user/file',
              'size': -1,
            },
          ],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertContext(lines, ROOT_DIR, expected, False)

    def test_futex_died(self):
      # That's a pretty bad fork, copy-pasted from a real log.
      lines = [
        (self._ROOT_PID, 'close(9)                                = 0'),
        (self._ROOT_PID, 'futex( <unfinished ... exit status 0>'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertContext(lines, ROOT_DIR, expected, True)

    def test_futex_missing_in_action(self):
      # That's how futex() calls roll.
      lines = [
        (self._ROOT_PID,
          'clone(child_stack=0x7fae9f4bed70, flags=CLONE_VM|CLONE_FS|'
          'CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|'
          'CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID, '
          'parent_tidptr=0x7fae9f4bf9d0, tls=0x7fae9f4bf700, '
          'child_tidptr=0x7fae9f4bf9d0) = 3862'),
        (self._ROOT_PID,
          'futex(0x1407670, FUTEX_WAIT_PRIVATE, 2, {0, 0}) = -1 EAGAIN '
          '(Resource temporarily unavailable)'),
        (self._ROOT_PID, 'futex(0x1407670, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'close(9)                                = 0'),
        (self._ROOT_PID, 'futex('),
      ]
      expected = {
        'root': {
          'children': [
            {
              'children': [],
              'command': None,
              'executable': None,
              'files': [],
              'initial_cwd': unicode(ROOT_DIR),
              'pid': 3862,
            },
          ],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertContext(lines, ROOT_DIR, expected, True)

    def test_futex_missing_in_partial_action(self):
      # That's how futex() calls roll even more.
      lines = [
        (self._ROOT_PID,
          'futex(0x7fff25718b14, FUTEX_CMP_REQUEUE_PRIVATE, 1, 2147483647, '
          '0x7fff25718ae8, 2) = 1'),
        (self._ROOT_PID, 'futex(0x7fff25718ae8, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 1, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 3, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 5, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 7, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x7f0c17780634, '
          'FUTEX_WAIT_BITSET_PRIVATE|FUTEX_CLOCK_REALTIME, 1, '
          '{1351180745, 913067000}, ffffffff'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertContext(lines, ROOT_DIR, expected, True)

    def test_futex_missing_in_partial_action_with_no_process(self):
      # That's how futex() calls roll even more (again).
      lines = [
          (self._ROOT_PID, 'futex(0x7134840, FUTEX_WAIT_PRIVATE, 2, '
           'NULL <ptrace(SYSCALL):No such process>'),
      ]
      expected = {
         'root': {
           'children': [],
           'command': None,
           'executable': None,
           'files': [],
           'initial_cwd': unicode(ROOT_DIR),
           'pid': self._ROOT_PID,
         },
       }
      self.assertContext(lines, ROOT_DIR, expected, True)

    def test_getcwd(self):
      lines = [
          (self._ROOT_PID, 'getcwd(0x7fffff0e13f0, 4096) = 52'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_lstat(self):
      lines = [
          (self._ROOT_PID, 'lstat(0x169a210, {...}) = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_open(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 8'),
        (self._ROOT_PID, 'open(0x7f68d954bb10, O_RDONLY|O_CLOEXEC) = 3'),
      ]
      files = [
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'mode': trace_inputs.Results.File.WRITE,
          'path': u'/home/foo_bar_user/src/out/unittests.log',
          'size': -1,
        },
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_open_resumed(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND '
            '<unfinished ...>'),
        (self._ROOT_PID, '<... open resumed> )              = 3'),
      ]
      files = [
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'mode': trace_inputs.Results.File.WRITE,
          'path': u'/home/foo_bar_user/src/out/unittests.log',
          'size': -1,
        },
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_openat(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
         'openat(AT_FDCWD, "/home/foo_bar_user/file", O_RDONLY) = 0'),
        (self._ROOT_PID,
         'openat(AT_FDCWD, 0xa23f60, '
           'O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC) = 4'),
      ]
      files = [
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/file',
          'size': -1,
        },
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
      ]

      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_openat_died(self):
      lines = [
          # It's fine as long as there is nothing after.
        ( self._ROOT_PID,
          'openat(AT_FDCWD, "/tmp/random_dir/Plugins", '
          'O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC'),
      ]
      expected = {
         'root': {
           'children': [],
           'command': None,
           'executable': None,
           'files': [],
           'initial_cwd': unicode(ROOT_DIR),
           'pid': self._ROOT_PID,
         },
       }
      self.assertContext(lines, ROOT_DIR, expected, True)

    def test_readlink(self):
      lines = [
          (self._ROOT_PID, 'readlink(0x941e60, 0x7fff7a632d60, 4096) = 9'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_stat(self):
      lines = [
          (self._ROOT_PID,
            'stat(0x941e60, {st_mode=S_IFREG|0644, st_size=25769, ...}) = 0'),
          (self._ROOT_PID, 'stat(0x941e60, {...}) = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_rmdir(self):
      lines = [
          (self._ROOT_PID, 'rmdir("directory/to/delete") = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_setxattr(self):
      lines = [
          (self._ROOT_PID,
           'setxattr("file.exe", "attribute", "value", 0, 0) = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_sig_unexpected(self):
      lines = [
        (self._ROOT_PID, 'exit_group(0)                     = ?'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_stray(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          ')                                       = ? <unavailable>'),
      ]
      files = [
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_truncate(self):
      lines = [
          (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
          (self._ROOT_PID,
           'truncate("file.exe", 0) = 0'),
      ]
      files = [
        {
          'mode': trace_inputs.Results.File.READ,
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'mode': trace_inputs.Results.File.WRITE,
          'path': u'/home/foo_bar_user/src/file.exe',
          'size': -1,
        },
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_vfork(self):
      # vfork is the only function traced that doesn't take parameters.
      lines = [
          (self._ROOT_PID, 'vfork() = %d' % self._CHILD_PID),
      ]
      expected = {
         'root': {
           'children': [
             {
               'children': [],
               'command': None,
               'executable': None,
               'files': [],
                'initial_cwd': unicode(ROOT_DIR),
               'pid': self._CHILD_PID,
              }
            ],
           'command': None,
           'executable': None,
           'files': [],
           'initial_cwd': unicode(ROOT_DIR),
           'pid': self._ROOT_PID,
         },
       }
      self.assertContext(lines, ROOT_DIR, expected, False)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
