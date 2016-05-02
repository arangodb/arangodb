#!/usr/bin/env python
# Copyright 2015 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Calculate statistics about tasks.

Saves the data fetched from the server into a json file to enable reprocessing
the data without having to always fetch from the server.
"""

import datetime
import json
import logging
import optparse
import os
import subprocess
import sys
import urllib


CLIENT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

_EPOCH = datetime.datetime.utcfromtimestamp(0)

# Type of bucket to use.
MAJOR_OS, MINOR_OS, MINOR_OS_GPU = range(3)


def seconds_to_timedelta(seconds):
  """Converts seconds in datetime.timedelta, stripping sub-second precision.

  This is for presentation, where subsecond values for summaries is not useful.
  """
  return datetime.timedelta(seconds=round(seconds))


def parse_time_option(value):
  """Converts time as an option into a datetime.datetime.

  Returns None if not specified.
  """
  if not value:
    return None
  try:
    return _EPOCH + datetime.timedelta(seconds=int(value))
  except ValueError:
    pass
  for fmt in (
      '%Y-%m-%d',
      '%Y-%m-%d %H:%M',
      '%Y-%m-%dT%H:%M',
      '%Y-%m-%d %H:%M:%S',
      '%Y-%m-%dT%H:%M:%S',
      '%Y-%m-%d %H:%M:%S.%f',
      '%Y-%m-%dT%H:%M:%S.%f'):
    try:
      return datetime.datetime.strptime(value, fmt)
    except ValueError:
      pass
  raise ValueError('Failed to parse %s' % value)


def parse_time(value):
  """Converts serialized time from the API to datetime.datetime."""
  for fmt in ('%Y-%m-%dT%H:%M:%S.%f', '%Y-%m-%dT%H:%M:%S'):
    try:
      return datetime.datetime.strptime(value, fmt)
    except ValueError:
      pass
  raise ValueError('Failed to parse %s' % value)


def average(items):
  if not items:
    return 0.
  return sum(items) / len(items)


def median(items):
  return percentile(items, 50)


def percentile(items, percent):
  """Uses NIST method."""
  if not items:
    return 0.
  rank = percent * .01 * (len(items) + 1)
  rank_int = int(rank)
  rest = rank - rank_int
  if rest and rank_int <= len(items) - 1:
    return items[rank_int] + rest * (items[rank_int+1] - items[rank_int])
  return items[min(rank_int, len(items) - 1)]


def sp(dividend, divisor):
  """Returns the percentage for dividend/divisor, safely."""
  if not divisor:
    return 0.
  return 100. * float(dividend) / float(divisor)


def fetch_data(options):
  """Fetches data from options.swarming and writes it to options.json."""
  cmd = [
    sys.executable, os.path.join(CLIENT_DIR, 'swarming.py'),
    'query',
    '-S', options.swarming,
    '--json', options.json,
    # Start chocking at 1m bots. The chromium infrastructure is currently at
    # around thousands range.
    '--limit', '1000000',
    '--progress',
    'bots/list',
  ]
  if options.verbose:
    cmd.append('--verbose')
    cmd.append('--verbose')
    cmd.append('--verbose')
  logging.info('%s', ' '.join(cmd))
  subprocess.check_call(cmd)
  print('')


def present_data(bots, bucket_type, order_count):
  buckets = do_bucket(bots, bucket_type)
  maxlen = max(len(i) for i in buckets)
  print('%-*s  Alive  Dead' % (maxlen, 'Type'))
  counts = {
      k: [len(v), sum(1 for i in v if i.get('is_dead'))]
        for k, v in buckets.iteritems()}
  key = (lambda x: -x[1][0]) if order_count else (lambda x: x)
  for bucket, count in sorted(counts.iteritems(), key=key):
    print('%-*s: %5d %5d' % (maxlen, bucket, count[0], count[1]))


def do_bucket(bots, bucket_type):
  """Categorizes the bots based on one of the bucket type defined above."""
  out = {}
  for bot in bots:
    # Convert dimensions from list of StringPairs to dict of list.
    bot['dimensions'] = {i['key']: i['value'] for i in bot['dimensions']}
    os_types = bot['dimensions']['os']
    try:
      os_types.remove('Linux')
    except ValueError:
      pass
    if bucket_type == MAJOR_OS:
      bucket = os_types[0]
    else:
      bucket = ' & '.join(os_types[1:])
    if bucket_type == MINOR_OS_GPU:
      gpu = bot['dimensions'].get('gpu', ['none'])[-1]
      if gpu != 'none':
        bucket += ' ' + gpu
    out.setdefault(bucket, []).append(bot)
  return out


def main():
  parser = optparse.OptionParser(description=sys.modules['__main__'].__doc__)
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
      help='Swarming server to use')
  parser.add_option(
      '--json', default='fleet.json',
      help='File containing raw data; default: %default')
  parser.add_option('-v', '--verbose', action='count', default=0)
  parser.add_option('--count', action='store_true', help='Order by count')

  group = optparse.OptionGroup(parser, 'Grouping')
  group.add_option(
      '--major-os', action='store_const',
      dest='bucket', const=MAJOR_OS,
      help='Classify by OS type, independent of OS version')
  group.add_option(
      '--minor-os', action='store_const',
      dest='bucket', const=MINOR_OS,
      help='Classify by minor OS version')
  group.add_option(
      '--gpu', action='store_const',
      dest='bucket', const=MINOR_OS_GPU, default=MINOR_OS_GPU,
      help='Classify by minor OS version and GPU type when requested (default)')
  parser.add_option_group(group)

  options, args = parser.parse_args()

  if args:
    parser.error('Unsupported argument %s' % args)
  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)
  if options.swarming:
    fetch_data(options)
  elif not os.path.isfile(options.json):
    parser.error('--swarming is required.')

  with open(options.json, 'rb') as f:
    items = json.load(f)['items']
  present_data(items, options.bucket, options.count)
  return 0


if __name__ == '__main__':
  sys.exit(main())
