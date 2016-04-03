# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os
import re

from third_party import colorama


UNITS = ('', 'k', 'm', 'g', 't', 'p', 'e', 'z', 'y')


def get_console_width(default=80):
  """Returns the console width, if available."""
  # TODO(maruel): Implement Windows.
  try:
    _, columns = os.popen('stty size', 'r').read().split()
  except (IOError, OSError, ValueError):
    columns = default
  return int(columns)


def generate_histogram(data, buckets):
  """Generates an histogram out of a list of floats.

  The data is bucketed into |buckets| buckets.

  Returns:
    dict of bucket: size
  """
  if not data:
    return {}

  minimum = min(data)
  maximum = max(data)
  if minimum == maximum:
    return {data[0]: len(data)}

  buckets = min(len(data), buckets)
  bucket_size = (maximum-minimum)/buckets
  out = dict((i, 0) for i in xrange(buckets))
  for i in data:
    out[min(int((i-minimum)/bucket_size), buckets-1)] += 1
  return dict(((k*bucket_size)+minimum, v) for k, v in out.iteritems())


def print_histogram(data, columns=0, key_format=None):
  """Prints ASCII art representing an histogram.

  Arguments:
    data: as formatted by generate_histogram().
    columns: width of the graph.
    key_format: printf like format for the keys.
  """
  # TODO(maruel): Add dots for tens.
  if not data:
    # Nothing to print.
    return

  columns = columns or get_console_width()
  key_format = key_format or '%s'

  max_key_width = max(len(key_format % k) for k in data)
  # 3 == 1 for ' ' prefix, 2 for ': ' suffix.
  width = columns - max_key_width - 3
  assert width > 1

  maxvalue = max(data.itervalues())
  if all(isinstance(i, int) for i in data.itervalues()) and maxvalue < width:
    width = maxvalue
  norm = float(maxvalue) / width

  form = '%s: %s%s%s'
  for k in sorted(data):
    line = '*' * int(data[k] / norm)
    key = (key_format % k).rjust(max_key_width)
    print(form % (key, colorama.Fore.GREEN, line, colorama.Fore.RESET))


def to_units(number):
  """Convert a string to numbers."""
  unit = 0
  while number >= 1024.:
    unit += 1
    number = number / 1024.
    if unit == len(UNITS) - 1:
      break
  if unit:
    return '%.2f%s' % (number, UNITS[unit])
  return '%d' % number


def from_units(text):
  """Convert a text to numbers.

  Example: from_unit('0.1k') == 102
  """
  match = re.match(r'^([0-9\.]+)(|[' + ''.join(UNITS[1:]) + r'])$', text)
  if not match:
    return None

  number = float(match.group(1))
  unit = match.group(2)
  return int(number * 1024**UNITS.index(unit))


def unit_arg(option, opt, value, parser):
  """OptionParser callback that supports units like 10.5m or 20k."""
  actual = from_units(value)
  if actual is None:
    parser.error('Invalid value \'%s\' for %s' % (value, opt))
  setattr(parser.values, option.dest, actual)


def unit_option(parser, *args, **kwargs):
  """Add an option that uses unit_arg()."""
  parser.add_option(
      *args, type='str', metavar='N', action='callback', callback=unit_arg,
      nargs=1, **kwargs)
