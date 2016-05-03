# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Intelligent natural sort implementation."""

import re


def natcmp(a, b):
  """Natural string comparison, case sensitive."""
  try_int = lambda s: int(s) if s.isdigit() else s
  def natsort_key(s):
    if not isinstance(s, basestring):
      # Since re.findall() generates a list out of a string, returns a list here
      # to balance the comparison done in cmp().
      return [s]
    return map(try_int, re.findall(r'(\d+|\D+)', s))
  return cmp(natsort_key(a), natsort_key(b))


def try_lower(x):
  """Opportunistically lower() a string if it is a string."""
  return x.lower() if hasattr(x, 'lower') else x


def naticasecmp(a, b):
  """Natural string comparison, ignores case."""
  return natcmp(try_lower(a), try_lower(b))


def natsort(seq, cmp=natcmp, *args, **kwargs):  # pylint: disable=W0622
  """In-place natural string sort.
  >>> a = ['3A2', '3a1']
  >>> natsort(a, key=try_lower)
  >>> a
  ['3a1', '3A2']
  >>> a = ['3a2', '3A1']
  >>> natsort(a, key=try_lower)
  >>> a
  ['3A1', '3a2']
  >>> a = ['3A2', '3a1']
  >>> natsort(a, cmp=naticasecmp)
  >>> a
  ['3a1', '3A2']
  >>> a = ['3a2', '3A1']
  >>> natsort(a, cmp=naticasecmp)
  >>> a
  ['3A1', '3a2']
  """
  seq.sort(cmp=cmp, *args, **kwargs)


def natsorted(seq, cmp=natcmp, *args, **kwargs):  # pylint: disable=W0622
  """Returns a copy of seq, sorted by natural string sort.

  >>> natsorted(i for i in [4, '3a', '2', 1])
  [1, '2', '3a', 4]
  >>> natsorted(['a4', 'a30'])
  ['a4', 'a30']
  >>> natsorted(['3A2', '3a1'], key=try_lower)
  ['3a1', '3A2']
  >>> natsorted(['3a2', '3A1'], key=try_lower)
  ['3A1', '3a2']
  >>> natsorted(['3A2', '3a1'], cmp=naticasecmp)
  ['3a1', '3A2']
  >>> natsorted(['3a2', '3A1'], cmp=naticasecmp)
  ['3A1', '3a2']
  >>> natsorted(['3A2', '3a1'])
  ['3A2', '3a1']
  >>> natsorted(['3a2', '3A1'])
  ['3A1', '3a2']
  """
  return sorted(seq, cmp=cmp, *args, **kwargs)
