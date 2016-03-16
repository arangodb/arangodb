#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import json
import logging
import os
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import lru


class LRUDictTest(unittest.TestCase):
  @staticmethod
  def prepare_lru_dict(keys):
    """Returns new LRUDict with given |keys| added one by one."""
    lru_dict = lru.LRUDict()
    for key in keys:
      lru_dict.add(key, None)
    return lru_dict

  def assert_order(self, lru_dict, expected_keys):
    """Asserts order of keys in |lru_dict| is |expected_keys|.

    expected_keys[0] is supposedly oldest key, expected_keys[-1] is newest.

    Destroys |lru_dict| state in the process.
    """
    # Check keys iteration works.
    self.assertEqual(lru_dict.keys_set(), set(expected_keys))

    # Check pop_oldest returns keys in expected order.
    actual_keys = []
    while lru_dict:
      oldest_key, _ = lru_dict.pop_oldest()
      actual_keys.append(oldest_key)
    self.assertEqual(actual_keys, expected_keys)

  def assert_same_data(self, lru_dict, regular_dict):
    """Asserts that given |lru_dict| contains same data as |regular_dict|."""
    self.assertEqual(lru_dict.keys_set(), set(regular_dict.keys()))
    self.assertEqual(set(lru_dict.itervalues()), set(regular_dict.values()))

    for k, v in regular_dict.items():
      self.assertEqual(lru_dict.get(k), v)

  def test_basic_dict_funcs(self):
    lru_dict = lru.LRUDict()

    # Add a bunch.
    data = {1: 'one', 2: 'two', 3: 'three'}
    for k, v in data.items():
      lru_dict.add(k, v)
    # Check its there.
    self.assert_same_data(lru_dict, data)

    # Replace value.
    lru_dict.add(1, 'one!!!')
    data[1] = 'one!!!'
    self.assert_same_data(lru_dict, data)

    # Check pop works.
    self.assertEqual(lru_dict.pop(2), 'two')
    data.pop(2)
    self.assert_same_data(lru_dict, data)

    # Pop missing key.
    with self.assertRaises(KeyError):
      lru_dict.pop(2)

    # Touch has no effect on set of keys and values.
    lru_dict.touch(1)
    self.assert_same_data(lru_dict, data)

    # Touch fails on missing key.
    with self.assertRaises(KeyError):
      lru_dict.touch(22)

  def test_magic_methods(self):
    # Check __nonzero__, __len__ and __contains__ for empty dict.
    lru_dict = lru.LRUDict()
    self.assertFalse(lru_dict)
    self.assertEqual(len(lru_dict), 0)
    self.assertFalse(1 in lru_dict)

    # Dict with one item.
    lru_dict.add(1, 'one')
    self.assertTrue(lru_dict)
    self.assertEqual(len(lru_dict), 1)
    self.assertTrue(1 in lru_dict)
    self.assertFalse(2 in lru_dict)

  def test_order(self):
    data = [1, 2, 3]

    # Edge cases.
    self.assert_order(self.prepare_lru_dict([]), [])
    self.assert_order(self.prepare_lru_dict([1]), [1])

    # No touches.
    self.assert_order(self.prepare_lru_dict(data), data)

    # Touching newest item is noop.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.touch(3)
    self.assert_order(lru_dict, data)

    # Touch to move to newest.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.touch(2)
    self.assert_order(lru_dict, [1, 3, 2])

    # Pop newest.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.pop(1)
    self.assert_order(lru_dict, [2, 3])

    # Pop in the middle.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.pop(2)
    self.assert_order(lru_dict, [1, 3])

    # Pop oldest.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.pop(3)
    self.assert_order(lru_dict, [1, 2])

    # Add oldest.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.batch_insert_oldest([(4, 4), (5, 5)])
    self.assert_order(lru_dict, [4, 5] + data)

    # Add newest.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.add(4, 4)
    self.assert_order(lru_dict, data + [4])

  def test_load_save(self):
    def save_and_load(lru_dict):
      handle, tmp_name = tempfile.mkstemp(prefix=u'lru_test')
      os.close(handle)
      try:
        lru_dict.save(tmp_name)
        return lru.LRUDict.load(tmp_name)
      finally:
        try:
          os.unlink(tmp_name)
        except OSError:
          pass

    data = [1, 2, 3]

    # Edge case.
    empty = save_and_load(lru.LRUDict())
    self.assertFalse(empty)

    # Normal flow.
    lru_dict = save_and_load(self.prepare_lru_dict(data))
    self.assert_order(lru_dict, data)

    # After touches.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.touch(2)
    lru_dict = save_and_load(lru_dict)
    self.assert_order(lru_dict, [1, 3, 2])

    # After pop.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.pop(2)
    lru_dict = save_and_load(lru_dict)
    self.assert_order(lru_dict, [1, 3])

    # After add.
    lru_dict = self.prepare_lru_dict(data)
    lru_dict.add(4, 4)
    lru_dict.batch_insert_oldest([(5, 5), (6, 6)])
    lru_dict = save_and_load(lru_dict)
    self.assert_order(lru_dict, [5, 6] + data + [4])

  def test_corrupted_state_file(self):
    def load_from_state(state_text):
      handle, tmp_name = tempfile.mkstemp(prefix=u'lru_test')
      os.close(handle)
      try:
        with open(tmp_name, 'w') as f:
          f.write(state_text)
        return lru.LRUDict.load(tmp_name)
      finally:
        os.unlink(tmp_name)

    # Loads correct state just fine.
    self.assertIsNotNone(load_from_state(json.dumps([
        ['key1', 'value1'],
        ['key2', 'value2'],
    ])))

    # Not a json.
    with self.assertRaises(ValueError):
      load_from_state('garbage, not a state')

    # Not a list.
    with self.assertRaises(ValueError):
      load_from_state('{}')

    # Not a list of pairs.
    with self.assertRaises(ValueError):
      load_from_state(json.dumps([
          ['key', 'value', 'and whats this?'],
      ]))

    # Duplicate keys.
    with self.assertRaises(ValueError):
      load_from_state(json.dumps([
          ['key', 'value'],
          ['key', 'another_value'],
      ]))


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
