# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import operator


def freeze(obj):
  """Takes a generic object ``obj``, and returns an immutable version of it.

  Supported types:
    * dict / OrderedDict -> FrozenDict
    * list -> tuple
    * set -> frozenset
    * any object with a working __hash__ implementation (assumes that hashable
      means immutable)

  Will raise TypeError if you pass an object which is not hashable.
  """
  if isinstance(obj, dict):
    return FrozenDict((freeze(k), freeze(v)) for k, v in obj.iteritems())
  elif isinstance(obj, (list, tuple)):
    return tuple(freeze(i) for i in obj)
  elif isinstance(obj, set):
    return frozenset(freeze(i) for i in obj)
  else:
    hash(obj)
    return obj


def thaw(obj):
  """Takes an object from freeze() and returns a mutable copy of it."""
  if isinstance(obj, FrozenDict):
    return collections.OrderedDict(
        (thaw(k), thaw(v)) for k, v in obj.iteritems())
  elif isinstance(obj, tuple):
    return list(thaw(i) for i in obj)
  elif isinstance(obj, frozenset):
    return set(thaw(i) for i in obj)
  else:
    return obj


class FrozenDict(collections.Mapping):
  """An immutable OrderedDict.

  Modified From: http://stackoverflow.com/a/2704866
  """
  def __init__(self, *args, **kwargs):
    self._d = collections.OrderedDict(*args, **kwargs)

    # Calculate the hash immediately so that we know all the items are
    # hashable too.
    self._hash = reduce(operator.xor,
                        (hash(i) for i in enumerate(self._d.iteritems())), 0)

  def __eq__(self, other):
    if not isinstance(other, collections.Mapping):
      return NotImplemented
    if self is other:
      return True
    if len(self) != len(other):
      return False
    for k, v in self.iteritems():
      if k not in other or other[k] != v:
        return False
    return True

  def __iter__(self):
    return iter(self._d)

  def __len__(self):
    return len(self._d)

  def __getitem__(self, key):
    return self._d[key]

  def __hash__(self):
    return self._hash

  def __repr__(self):
    return 'FrozenDict(%r)' % (self._d.items(),)
