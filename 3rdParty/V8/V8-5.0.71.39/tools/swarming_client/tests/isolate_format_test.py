#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import cStringIO
import logging
import os
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

import isolate_format
from depot_tools import auto_stub
from depot_tools import fix_encoding
from utils import file_path


# Access to a protected member XXX of a client class
# pylint: disable=W0212


FAKE_DIR = (
    u'z:\\path\\to\\non_existing'
    if sys.platform == 'win32' else u'/path/to/non_existing')


class IsolateFormatTest(auto_stub.TestCase):
  def test_unknown_key(self):
    try:
      isolate_format.verify_variables({'foo': [],})
      self.fail()
    except AssertionError:
      pass

  def test_unknown_var(self):
    try:
      isolate_format.verify_condition({'variables': {'foo': [],}}, {})
      self.fail()
    except AssertionError:
      pass

  def test_eval_content(self):
    try:
      # Intrinsics are not available.
      isolate_format.eval_content('map(str, [1, 2])')
      self.fail()
    except NameError:
      pass

  def test_load_isolate_as_config_empty(self):
    expected = {
      (): {
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(
        expected,
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None).flatten())

  def test_load_isolate_as_config(self):
    value = {
      'conditions': [
        ['OS=="amiga" or OS=="atari" or OS=="coleco" or OS=="dendy"', {
          'variables': {
            'files': ['a', 'b', 'touched'],
          },
        }],
        ['OS=="atari"', {
          'variables': {
            'files': ['c', 'd', 'touched_a', 'x'],
            'command': ['echo', 'Hello World'],
            'read_only': 2,
          },
        }],
        ['OS=="amiga" or OS=="coleco" or OS=="dendy"', {
          'variables': {
            'files': ['e', 'f', 'touched_e', 'x'],
            'command': ['echo', 'You should get an Atari'],
          },
        }],
        ['OS=="amiga"', {
          'variables': {
            'files': ['g'],
            'read_only': 1,
          },
        }],
        ['OS=="amiga" or OS=="atari" or OS=="dendy"', {
          'variables': {
            'files': ['h'],
          },
        }],
      ],
    }
    expected = {
      (None,): {
        'isolate_dir': FAKE_DIR,
      },
      ('amiga',): {
        'files': ['a', 'b', 'e', 'f', 'g', 'h', 'touched', 'touched_e', 'x'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
        'read_only': 1,
      },
      ('atari',): {
        'files': ['a', 'b', 'c', 'd', 'h', 'touched', 'touched_a', 'x'],
        'command': ['echo', 'Hello World'],
        'isolate_dir': FAKE_DIR,
        'read_only': 2,
      },
      ('coleco',): {
        'files': ['a', 'b', 'e', 'f', 'touched', 'touched_e', 'x'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
      },
      ('dendy',): {
        'files': ['a', 'b', 'e', 'f', 'h', 'touched', 'touched_e', 'x'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(
        expected, isolate_format.load_isolate_as_config(
            FAKE_DIR, value, None).flatten())

  def test_load_isolate_as_config_duplicate_command(self):
    value = {
      'variables': {
        'command': ['rm', '-rf', '/'],
      },
      'conditions': [
        ['OS=="atari"', {
          'variables': {
            'command': ['echo', 'Hello World'],
          },
        }],
      ],
    }
    try:
      isolate_format.load_isolate_as_config(FAKE_DIR, value, None)
      self.fail()
    except AssertionError:
      pass

  def test_load_isolate_as_config_no_variable(self):
    value = {
      'variables': {
        'command': ['echo', 'You should get an Atari'],
        'files': ['a', 'b', 'touched'],
        'read_only': 1,
      },
    }
    # The key is the empty tuple, since there is no variable to bind to.
    expected = {
      (): {
        'command': ['echo', 'You should get an Atari'],
        'files': ['a', 'b', 'touched'],
        'isolate_dir': FAKE_DIR,
        'read_only': 1,
      },
    }
    self.assertEqual(
        expected, isolate_format.load_isolate_as_config(
            FAKE_DIR, value, None).flatten())

  def test_merge_two_empty(self):
    # Flat stay flat. Pylint is confused about union() return type.
    # pylint: disable=E1103
    actual = isolate_format.Configs(None, ()).union(
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None)).union(
            isolate_format.load_isolate_as_config(FAKE_DIR, {}, None))
    expected = {
      (): {
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(expected, actual.flatten())

  def test_load_two_conditions(self):
    linux = {
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'files': [
              'file_linux',
              'file_common',
            ],
          },
        }],
      ],
    }
    mac = {
      'conditions': [
        ['OS=="mac"', {
          'variables': {
            'files': [
              'file_mac',
              'file_common',
            ],
          },
        }],
      ],
    }
    expected = {
      (None,): {
        'isolate_dir': FAKE_DIR,
      },
      ('linux',): {
        'files': ['file_common', 'file_linux'],
        'isolate_dir': FAKE_DIR,
      },
      ('mac',): {
        'files': ['file_common', 'file_mac'],
        'isolate_dir': FAKE_DIR,
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = isolate_format.Configs(None, ()).union(
        isolate_format.load_isolate_as_config(FAKE_DIR, linux, None)).union(
            isolate_format.load_isolate_as_config(FAKE_DIR, mac, None)
        ).flatten()
    self.assertEqual(expected, configs)

  def test_load_three_conditions(self):
    linux = {
      'conditions': [
        ['OS=="linux" and chromeos==1', {
          'variables': {
            'files': [
              'file_linux',
              'file_common',
            ],
          },
        }],
      ],
    }
    mac = {
      'conditions': [
        ['OS=="mac" and chromeos==0', {
          'variables': {
            'files': [
              'file_mac',
              'file_common',
            ],
          },
        }],
      ],
    }
    win = {
      'conditions': [
        ['OS=="win" and chromeos==0', {
          'variables': {
            'files': [
              'file_win',
              'file_common',
            ],
          },
        }],
      ],
    }
    expected = {
      (None, None): {
        'isolate_dir': FAKE_DIR,
      },
      ('linux', 1): {
        'files': ['file_common', 'file_linux'],
        'isolate_dir': FAKE_DIR,
      },
      ('mac', 0): {
        'files': ['file_common', 'file_mac'],
        'isolate_dir': FAKE_DIR,
      },
      ('win', 0): {
        'files': ['file_common', 'file_win'],
        'isolate_dir': FAKE_DIR,
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = isolate_format.Configs(None, ()).union(
        isolate_format.load_isolate_as_config(FAKE_DIR, linux, None)).union(
            isolate_format.load_isolate_as_config(FAKE_DIR, mac, None)).union(
                isolate_format.load_isolate_as_config(FAKE_DIR, win, None))
    self.assertEqual(expected, configs.flatten())

  def test_safe_index(self):
    self.assertEqual(1, isolate_format._safe_index(('a', 'b'), 'b'))
    self.assertEqual(None, isolate_format._safe_index(('a', 'b'), 'c'))

  def test_get_map_keys(self):
    self.assertEqual(
        (0, None, 1), isolate_format._get_map_keys(('a', 'b', 'c'), ('a', 'c')))

  def test_map_keys(self):
    self.assertEqual(
        ('a', None, 'c'),
        isolate_format._map_keys((0, None, 1), ('a', 'c')))

  def test_load_multi_variables(self):
    # Load an .isolate with different condition on different variables.
    data = {
      'conditions': [
        ['OS=="abc"', {
          'variables': {
            'command': ['bar'],
          },
        }],
        ['CHROMEOS=="1"', {
          'variables': {
            'command': ['foo'],
          },
        }],
      ],
    }
    configs = isolate_format.load_isolate_as_config(FAKE_DIR, data, None)
    self.assertEqual(('CHROMEOS', 'OS'), configs.config_variables)
    flatten = dict((k, v.flatten()) for k, v in configs._by_config.iteritems())
    expected = {
      (None, None): {
        'isolate_dir': FAKE_DIR,
      },
      (None, 'abc'): {
        'command': ['bar'],
        'isolate_dir': FAKE_DIR,
      },
      ('1', None): {
        'command': ['foo'],
        'isolate_dir': FAKE_DIR,
      },
      # TODO(maruel): It is a conflict.
      ('1', 'abc'): {
        'command': ['bar'],
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(expected, flatten)

  def test_union_multi_variables(self):
    data1 = {
      'conditions': [
        ['OS=="abc"', {
          'variables': {
            'command': ['bar'],
          },
        }],
      ],
    }
    data2 = {
      'conditions': [
        ['CHROMEOS=="1"', {
          'variables': {
            'command': ['foo'],
          },
        }],
      ],
    }
    configs1 = isolate_format.load_isolate_as_config(FAKE_DIR, data1, None)
    configs2 = isolate_format.load_isolate_as_config(FAKE_DIR, data2, None)
    configs = configs1.union(configs2)
    self.assertEqual(('CHROMEOS', 'OS'), configs.config_variables)
    flatten = dict((k, v.flatten()) for k, v in configs._by_config.iteritems())
    expected = {
      (None, None): {
        'isolate_dir': FAKE_DIR,
      },
      (None, 'abc'): {
        'command': ['bar'],
        'isolate_dir': FAKE_DIR,
      },
      ('1', None): {
        'command': ['foo'],
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(expected, flatten)

  def test_ConfigSettings_union(self):
    lhs_values = {}
    rhs_values = {'files': ['data/', 'test/data/']}
    lhs = isolate_format.ConfigSettings(lhs_values, '/src/net/third_party/nss')
    rhs = isolate_format.ConfigSettings(rhs_values, '/src/base')
    out = lhs.union(rhs)
    expected = {
      'files': ['data/', 'test/data/'],
      'isolate_dir': '/src/base',
    }
    self.assertEqual(expected, out.flatten())

  def test_configs_comment(self):
    # Pylint is confused with isolate_format.union() return type.
    # pylint: disable=E1103
    configs = isolate_format.load_isolate_as_config(
            FAKE_DIR, {}, '# Yo dawg!\n# Chill out.\n').union(
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None))
    self.assertEqual('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    configs = isolate_format.load_isolate_as_config(FAKE_DIR, {}, None).union(
        isolate_format.load_isolate_as_config(
            FAKE_DIR, {}, '# Yo dawg!\n# Chill out.\n'))
    self.assertEqual('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    # Only keep the first one.
    configs = isolate_format.load_isolate_as_config(
        FAKE_DIR, {}, '# Yo dawg!\n').union(
            isolate_format.load_isolate_as_config(
                FAKE_DIR, {}, '# Chill out.\n'))
    self.assertEqual('# Yo dawg!\n', configs.file_comment)

  def test_extract_comment(self):
    self.assertEqual(
        '# Foo\n# Bar\n', isolate_format.extract_comment('# Foo\n# Bar\n{}'))
    self.assertEqual('', isolate_format.extract_comment('{}'))

  def _test_pretty_print_impl(self, value, expected):
    actual = cStringIO.StringIO()
    isolate_format.pretty_print(value, actual)
    self.assertEqual(expected.splitlines(), actual.getvalue().splitlines())

  def test_pretty_print_empty(self):
    self._test_pretty_print_impl({}, '{\n}\n')

  def test_pretty_print_mid_size(self):
    value = {
      'variables': {
        'files': [
          'file1',
          'file2',
        ],
      },
      'conditions': [
        ['OS==\"foo\"', {
          'variables': {
            'files': [
              'dir1/',
              'dir2/',
              'file3',
              'file4',
            ],
            'command': ['python', '-c', 'print "H\\i\'"'],
            'read_only': 2,
          },
        }],
        ['OS==\"bar\"', {
          'variables': {},
        }],
      ],
    }
    isolate_format.verify_root(value, {})
    # This is an .isolate format.
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'files': [\n"
        "      'file1',\n"
        "      'file2',\n"
        "    ],\n"
        "  },\n"
        "  'conditions': [\n"
        "    ['OS==\"foo\"', {\n"
        "      'variables': {\n"
        "        'command': [\n"
        "          'python',\n"
        "          '-c',\n"
        "          'print \"H\\i\'\"',\n"
        "        ],\n"
        "        'files': [\n"
        "          'dir1/',\n"
        "          'dir2/',\n"
        "          'file3',\n"
        "          'file4',\n"
        "        ],\n"
        "        'read_only': 2,\n"
        "      },\n"
        "    }],\n"
        "    ['OS==\"bar\"', {\n"
        "      'variables': {\n"
        "      },\n"
        "    }],\n"
        "  ],\n"
        "}\n")
    self._test_pretty_print_impl(value, expected)

  def test_convert_old_to_new_else(self):
    isolate_with_else_clauses = {
      'conditions': [
        ['OS=="mac"', {
          'variables': {'foo': 'bar'},
        }, {
          'variables': {'x': 'y'},
        }],
      ],
    }
    with self.assertRaises(isolate_format.IsolateError):
      isolate_format.load_isolate_as_config(
          FAKE_DIR, isolate_with_else_clauses, None)

  def test_match_configs(self):
    expectations = [
        (
          ('OS=="win"', ('OS',), [('win',), ('mac',), ('linux',)]),
          [('win',)],
        ),
        (
          (
            '(foo==1 or foo==2) and bar=="b"',
            ['foo', 'bar'],
            [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')],
          ),
          [(1, 'b'), (2, 'b')],
        ),
        (
          (
            'bar=="b"',
            ['foo', 'bar'],
            [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')],
          ),
          # TODO(maruel): When a free variable match is found, it should not
          # list all the bounded values in addition. The problem is when an
          # intersection of two different bound variables that are tested singly
          # in two different conditions.
          [(1, 'b'), (2, 'b'), (None, 'b')],
        ),
        (
          (
            'foo==1 or bar=="b"',
            ['foo', 'bar'],
            [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')],
          ),
          # TODO(maruel): (None, 'b') would match.
          # It is hard in this case to realize that each of the variables 'foo'
          # and 'bar' can be unbounded in a specific case.
          [(1, 'a'), (1, 'b'), (2, 'b'), (1, None)],
        ),
    ]
    for data, expected in expectations:
      self.assertEqual(expected, isolate_format.match_configs(*data))

  def test_load_with_globals(self):
    values = {
      'variables': {
        'files': [
          'file_common',
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'files': [
              'file_linux',
            ],
            'read_only': 1,
          },
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'files': [
              'file_non_linux',
            ],
            'read_only': 0,
          },
        }],
      ],
    }
    expected = {
      (None,): {
        'files': [
          'file_common',
        ],
        'isolate_dir': FAKE_DIR,
      },
      ('linux',): {
        'files': [
          'file_linux',
        ],
        'isolate_dir': FAKE_DIR,
        'read_only': 1,
      },
      ('mac',): {
        'files': [
          'file_non_linux',
        ],
        'isolate_dir': FAKE_DIR,
        'read_only': 0,
      },
      ('win',): {
        'files': [
          'file_non_linux',
        ],
        'isolate_dir': FAKE_DIR,
        'read_only': 0,
      },
    }
    actual = isolate_format.load_isolate_as_config(FAKE_DIR, values, None)
    self.assertEqual(expected, actual.flatten())

  def test_and_or_bug(self):
    a = {
      'conditions': [
        ['use_x11==0', {
          'variables': {
            'command': ['foo', 'x11=0'],
          },
        }],
        ['OS=="linux" and chromeos==0', {
          'variables': {
            'command': ['foo', 'linux'],
            },
          }],
        ],
      }

    def load_included_isolate(isolate_dir, _isolate_path):
      return isolate_format.load_isolate_as_config(isolate_dir, a, None)
    self.mock(isolate_format, 'load_included_isolate', load_included_isolate)

    b = {
      'conditions': [
        ['use_x11==1', {
          'variables': {
            'command': ['foo', 'x11=1'],
          },
        }],
      ],
      'includes': [
        'a',
      ],
    }
    variables = {'use_x11': 1, 'OS': 'linux', 'chromeos': 0}
    config = isolate_format.load_isolate_for_config('/', str(b), variables)
    self.assertEqual((['foo', 'x11=1'], [], None, '/'), config)
    variables = {'use_x11': 0, 'OS': 'linux', 'chromeos': 0}
    config = isolate_format.load_isolate_for_config('/', str(b), variables)
    self.assertEqual(([], [], None, '/'), config)


class IsolateFormatTmpDirTest(unittest.TestCase):
  def setUp(self):
    super(IsolateFormatTmpDirTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix=u'isolate_')

  def tearDown(self):
    try:
      file_path.rmtree(self.tempdir)
    finally:
      super(IsolateFormatTmpDirTest, self).tearDown()

  def test_load_with_includes(self):
    included_isolate = {
      'variables': {
        'files': [
          'file_common',
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'files': [
              'file_linux',
            ],
            'read_only': 1,
          },
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'files': [
              'file_non_linux',
            ],
            'read_only': 0,
          },
        }],
      ],
    }
    with open(os.path.join(self.tempdir, 'included.isolate'), 'wb') as f:
      isolate_format.pretty_print(included_isolate, f)
    values = {
      'includes': ['included.isolate'],
      'variables': {
        'files': [
          'file_less_common',
        ],
      },
      'conditions': [
        ['OS=="mac"', {
          'variables': {
            'files': [
              'file_mac',
            ],
            'read_only': 2,
          },
        }],
      ],
    }
    actual = isolate_format.load_isolate_as_config(self.tempdir, values, None)

    expected = {
      (None,): {
        'files': [
          'file_common',
          'file_less_common',
        ],
        'isolate_dir': self.tempdir,
      },
      ('linux',): {
        'files': [
          'file_linux',
        ],
        'isolate_dir': self.tempdir,
        'read_only': 1,
      },
      ('mac',): {
        'files': [
          'file_mac',
          'file_non_linux',
        ],
        'isolate_dir': self.tempdir,
        'read_only': 2,
      },
      ('win',): {
        'files': [
          'file_non_linux',
        ],
        'isolate_dir': self.tempdir,
        'read_only': 0,
      },
    }
    self.assertEqual(expected, actual.flatten())

  def test_load_with_includes_with_commands(self):
    # This one is messy. Check that isolate_dir is the expected value. To
    # achieve this, put the .isolate files into subdirectories.
    dir_1 = os.path.join(self.tempdir, '1')
    dir_3 = os.path.join(self.tempdir, '3')
    dir_3_2 = os.path.join(self.tempdir, '3', '2')
    os.mkdir(dir_1)
    os.mkdir(dir_3)
    os.mkdir(dir_3_2)

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
    # No need to write isolate3.
    with open(os.path.join(dir_1, 'isolate1.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate1, f)
    with open(os.path.join(dir_3_2, 'isolate2.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate2, f)

    # The 'isolate_dir' are important, they are what will be used when
    # definining the final isolate_dir to use to run the command in the
    # .isolated file.
    actual = isolate_format.load_isolate_as_config(dir_3, isolate3, None)
    expected = {
      (None,): {
        # TODO(maruel): See TODO in ConfigSettings.flatten().
        # TODO(maruel): If kept, in this case dir_3 should be selected.
        'isolate_dir': dir_1,
      },
      ('amiga',): {
        'command': ['foo', 'amiga_or_win'],
        'files': [
          # Note that the file was rebased from isolate1. This is important,
          # isolate1 represent the canonical root path because it is the one
          # that defined the command.
          '../3/file_amiga',
        ],
        'isolate_dir': dir_1,
      },
      ('linux',): {
        # Last included takes precedence. *command comes from isolate2*, so
        # it becomes the canonical root, so reference to file from isolate1 is
        # via '../../1'.
        'command': ['foo', 'linux_or_mac'],
        'files': [
          '../../1/file_linux',
          'other/file',
        ],
        'isolate_dir': dir_3_2,
      },
      ('mac',): {
        'command': ['foo', 'linux_or_mac'],
        'files': [
          '../../1/file_non_linux',
          '../file_mac',
          'other/file',
        ],
        'isolate_dir': dir_3_2,
      },
      ('win',): {
        # command comes from isolate1.
        'command': ['foo', 'amiga_or_win'],
        'files': [
          # While this may be surprising, this is because the command was
          # defined in isolate1, not isolate3.
          'file_non_linux',
        ],
        'isolate_dir': dir_1,
      },
    }
    self.assertEqual(expected, actual.flatten())

  def test_load_with_includes_with_commands_and_variables(self):
    # This one is the pinacle of fun. Check that isolate_dir is the expected
    # value. To achieve this, put the .isolate files into subdirectories.
    dir_1 = os.path.join(self.tempdir, '1')
    dir_3 = os.path.join(self.tempdir, '3')
    dir_3_2 = os.path.join(self.tempdir, '3', '2')
    os.mkdir(dir_1)
    os.mkdir(dir_3)
    os.mkdir(dir_3_2)

    isolate1 = {
      'conditions': [
        ['OS=="amiga" or OS=="win"', {
          'variables': {
            'command': [
              'foo', 'amiga_or_win', '<(PATH)',
            ],
          },
        }],
        ['OS=="linux"', {
          'variables': {
            'command': [
              'foo', 'linux', '<(PATH)',
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
              'foo', 'linux_or_mac', '<(PATH)',
            ],
            'files': [
              '<(PATH)/other/file',
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
              '<(PATH)/file_amiga',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'files': [
              '<(PATH)/file_mac',
            ],
          },
        }],
      ],
    }
    # No need to write isolate3.
    with open(os.path.join(dir_1, 'isolate1.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate1, f)
    with open(os.path.join(dir_3_2, 'isolate2.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate2, f)

    # The 'isolate_dir' are important, they are what will be used when
    # definining the final isolate_dir to use to run the command in the
    # .isolated file.
    actual = isolate_format.load_isolate_as_config(dir_3, isolate3, None)
    expected = {
      (None,): {
        'isolate_dir': dir_1,
      },
      ('amiga',): {
        'command': ['foo', 'amiga_or_win', '<(PATH)'],
        'files': [
          '<(PATH)/file_amiga',
        ],
        'isolate_dir': dir_1,
      },
      ('linux',): {
        # Last included takes precedence. *command comes from isolate2*, so
        # it becomes the canonical root, so reference to file from isolate1 is
        # via '../../1'.
        'command': ['foo', 'linux_or_mac', '<(PATH)'],
        'files': [
          '<(PATH)/file_linux',
          '<(PATH)/other/file',
        ],
        'isolate_dir': dir_3_2,
      },
      ('mac',): {
        'command': ['foo', 'linux_or_mac', '<(PATH)'],
        'files': [
          '<(PATH)/file_mac',
          '<(PATH)/file_non_linux',
          '<(PATH)/other/file',
        ],
        'isolate_dir': dir_3_2,
      },
      ('win',): {
        # command comes from isolate1.
        'command': ['foo', 'amiga_or_win', '<(PATH)'],
        'files': [
          '<(PATH)/file_non_linux',
        ],
        'isolate_dir': dir_1,
      },
    }
    self.assertEqual(expected, actual.flatten())


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
