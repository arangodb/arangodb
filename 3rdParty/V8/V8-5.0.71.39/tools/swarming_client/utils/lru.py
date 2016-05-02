# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Defines a dictionary that can evict least recently used items."""

import collections
import json


class LRUDict(object):
  """Dictionary that can evict least recently used items.

  Implemented as a wrapper around OrderedDict object. An OrderedDict stores
  (key, value) pairs in order they are inserted and can effectively pop oldest
  items.

  Can also store its state as *.json file on disk.
  """

  def __init__(self):
    # Ordered key -> value mapping, newest items at the bottom.
    self._items = collections.OrderedDict()
    # True if was modified after loading.
    self._dirty = True

  def __nonzero__(self):
    """False if dict is empty."""
    return bool(self._items)

  def __len__(self):
    """Number of items in the dict."""
    return len(self._items)

  def __contains__(self, key):
    """True if |key| is in the dict."""
    return key in self._items

  @classmethod
  def load(cls, state_file):
    """Loads previously saved state and returns LRUDict in that state.

    Raises ValueError if state file is corrupted.
    """
    try:
      state = json.load(open(state_file, 'r'))
    except (IOError, ValueError) as e:
      raise ValueError('Broken state file %s: %s' % (state_file, e))

    if not isinstance(state, list):
      raise ValueError(
          'Broken state file %s, should be json list' % (state_file,))

    # Items are stored oldest to newest. Put them back in the same order.
    lru = cls()
    for pair in state:
      if not isinstance(pair, (list, tuple)) or len(pair) != 2:
        raise ValueError(
           'Broken state file %s, expecting pairs: %s' % (state_file, pair))
      lru.add(pair[0], pair[1])

    # Check for duplicate keys.
    if len(lru) != len(state):
      raise ValueError(
          'Broken state file %s, found duplicate keys' % (state_file,))

    # Now state from the file corresponds to state in the memory.
    lru._dirty = False
    return lru

  def save(self, state_file):
    """Saves cache state to a file if it was modified."""
    if not self._dirty:
      return False

    with open(state_file, 'wb') as f:
      json.dump(self._items.items(), f, separators=(',',':'))

    self._dirty = False
    return True

  def add(self, key, value):
    """Adds or replaces a |value| for |key|, marks it as most recently used."""
    self._items.pop(key, None)
    self._items[key] = value
    self._dirty = True

  def batch_insert_oldest(self, items):
    """Prepends list of |items| to the dict, marks them as least recently used.

    |items| is a list of (key, value) pairs to add.

    It's a very slow operation that completely rebuilds the dictionary.
    """
    new_items = collections.OrderedDict()

    # Insert |items| first, so they became oldest.
    for key, value in items:
      new_items[key] = value

    # Insert the rest, be careful not to override keys from |items|.
    for key, value in self._items.iteritems():
      if key not in new_items:
        new_items[key] = value

    self._items = new_items
    self._dirty = True

  def keys_set(self):
    """Set of keys of items in this dict."""
    return set(self._items)

  def get(self, key, default=None):
    """Returns value for |key| or |default| if not found."""
    return self._items.get(key, default)

  def touch(self, key):
    """Marks |key| as most recently used.

    Raises KeyError if |key| is not in the dict.
    """
    self._items[key] = self._items.pop(key)
    self._dirty = True

  def pop(self, key):
    """Removes item from the dict, returns its value.

    Raises KeyError if |key| is not in the dict.
    """
    value = self._items.pop(key)
    self._dirty = True
    return value

  def pop_oldest(self):
    """Removes oldest item from the dict and returns it as tuple (key, value).

    Raises KeyError if dict is empty.
    """
    pair = self._items.popitem(last=False)
    self._dirty = True
    return pair

  def itervalues(self):
    """Iterator over stored values in arbitrary order."""
    return self._items.itervalues()
