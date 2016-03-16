#!/usr/bin/env python
# Copyright 2015 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Calculate statistics about tasks.

Saves the data fetched from the server into a json file to enable reprocessing
the data without having to always fetch from the server.
"""

import collections
import datetime
import json
import logging
import optparse
import os
import subprocess
import Queue
import threading
import sys
import urllib


CLIENT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, CLIENT_DIR)


from third_party import colorama
from utils import graph


_EPOCH = datetime.datetime.utcfromtimestamp(0)


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
  for fmt in ('%Y-%m-%d',):
    try:
      return datetime.datetime.strptime(value, fmt)
    except ValueError:
      pass
  raise ValueError('Failed to parse %s' % value)


def flatten(dimensions):
  items = {i['key']: i['value'] for i in dimensions}
  return ','.join('%s=%s' % (k, v) for k, v in sorted(items.iteritems()))


def fetch_tasks(swarming, start, end, state, tags):
  """Fetches the data."""
  def process(data):
    return [
        flatten(t['properties']['dimensions']) for t in data.get('items', [])]
  return _fetch_internal(
      swarming, process, 'tasks/requests', start, end, state, tags)


def fetch_counts(swarming, start, end, state, tags):
  """Fetches counts from swarming and returns it."""
  def process(data):
    return int(data['count'])
  return _fetch_internal(
      swarming, process, 'tasks/count', start, end, state, tags)


def _fetch_internal(swarming, process, endpoint, start, end, state, tags):
  # Split the work in days. That's a lot of requests to do.
  queue = Queue.Queue()
  threads = []
  def run(start, cmd):
    logging.info('Running %s', ' '.join(cmd))
    raw = subprocess.check_output(cmd)
    logging.info('- returned %d', len(raw))
    queue.put((start, process(json.loads(raw))))

  day = start
  while day != end:
    data = [
      ('start', int((day - _EPOCH).total_seconds())),
      ('end', int((day + datetime.timedelta(days=1)-_EPOCH).total_seconds())),
      ('state', state),
    ]
    for tag in tags:
      data.append(('tags', tag))
    cmd = [
      sys.executable, os.path.join(CLIENT_DIR, 'swarming.py'),
      'query', '-S', swarming,
      endpoint + '?' + urllib.urlencode(data),
    ]
    thread = threading.Thread(target=run, args=(day.strftime('%Y-%m-%d'), cmd))
    thread.daemon = True
    thread.start()
    threads.append(thread)
    while len(threads) > 100:
      # Throttle a bit.
      for i, thread in enumerate(threads):
        if not thread.is_alive():
          thread.join()
          threads.pop(i)
          sys.stdout.write('.')
          sys.stdout.flush()
          break
    day = day + datetime.timedelta(days=1)

  while threads:
    # Throttle a bit.
    for i, thread in enumerate(threads):
      if not thread.is_alive():
        thread.join()
        threads.pop(i)
        sys.stdout.write('.')
        sys.stdout.flush()
        break
  print('')
  data = []
  while True:
    try:
      data.append(queue.get_nowait())
    except Queue.Empty:
      break
  return dict(data)


def present_dimensions(items, daily_count):
  # Split items per group.
  per_dimension = collections.defaultdict(lambda: collections.defaultdict(int))
  for date, dimensions in items.iteritems():
    for d in dimensions:
      per_dimension[d][date] += 1
  for i, (dimension, data) in enumerate(sorted(per_dimension.iteritems())):
    print(
        '%s%s%s' % (
          colorama.Style.BRIGHT + colorama.Fore.MAGENTA,
          dimension,
          colorama.Fore.RESET))
    present_counts(data, daily_count)
    if i != len(per_dimension) - 1:
      print('')


def present_counts(items, daily_count):
  months = collections.defaultdict(int)
  for day, count in sorted(items.iteritems()):
    month = day.rsplit('-', 1)[0]
    months[month] += count

  years = collections.defaultdict(int)
  for month, count in months.iteritems():
    year = month.rsplit('-', 1)[0]
    years[year] += count
  total = sum(months.itervalues())
  maxlen = len(str(total))

  if daily_count:
    for day, count in sorted(items.iteritems()):
      print('%s: %*d' % (day, maxlen, count))

  if len(items) > 1:
    for month, count in sorted(months.iteritems()):
      print('%s   : %*d' % (month, maxlen, count))
  if len(months) > 1:
    for year, count in sorted(years.iteritems()):
      print('%s      : %*d' % (year, maxlen, count))
  if len(years) > 1:
    print('Total     : %*d' % (maxlen, total))
  if not daily_count:
    print('')
    graph.print_histogram(items)


STATES = (
    'PENDING',
    'RUNNING',
    'PENDING_RUNNING',
    'COMPLETED',
    'COMPLETED_SUCCESS',
    'COMPLETED_FAILURE',
    'EXPIRED',
    'TIMED_OUT',
    'BOT_DIED',
    'CANCELED',
    'ALL',
    'DEDUPED')


def main():
  colorama.init()
  parser = optparse.OptionParser(description=sys.modules['__main__'].__doc__)
  tomorrow = datetime.datetime.utcnow().date() + datetime.timedelta(days=1)
  year = datetime.datetime(tomorrow.year, 1, 1)
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
      help='Swarming server to use')
  group = optparse.OptionGroup(parser, 'Filtering')
  group.add_option(
      '--start', default=year.strftime('%Y-%m-%d'),
      help='Starting date in UTC; defaults to start of year: %default')
  group.add_option(
      '--end', default=tomorrow.strftime('%Y-%m-%d'),
      help='End date in UTC; defaults to tomorrow: %default')
  group.add_option(
      '--state', default='ALL', type='choice', choices=STATES,
      help='State to filter on. Values are: %s' % ', '.join(STATES))
  group.add_option(
      '--tags', action='append', default=[], help='Tags to filter on')
  parser.add_option_group(group)
  group = optparse.OptionGroup(parser, 'Presentation')
  group.add_option(
      '--dimensions', action='store_true', help='Show the dimensions')
  group.add_option(
      '--daily-count', action='store_true',
      help='Show the daily count in raw number instead of histogram')
  parser.add_option_group(group)
  parser.add_option(
      '--json', default='counts.json',
      help='File containing raw data; default: %default')
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Log')
  options, args = parser.parse_args()

  if args:
    parser.error('Unsupported argument %s' % args)
  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)
  start = parse_time_option(options.start)
  end = parse_time_option(options.end)
  print('From %s (%d) to %s (%d)' % (
      start, int((start- _EPOCH).total_seconds()),
      end, int((end - _EPOCH).total_seconds())))
  if options.swarming:
    if options.dimensions:
      data = fetch_tasks(
          options.swarming, start, end, options.state, options.tags)
    else:
      data = fetch_counts(
          options.swarming, start, end, options.state, options.tags)
    with open(options.json, 'wb') as f:
      json.dump(data, f)
  elif not os.path.isfile(options.json):
    parser.error('--swarming is required.')
  else:
    with open(options.json, 'rb') as f:
      data = json.load(f)

  print('')
  if options.dimensions:
    present_dimensions(data, options.daily_count)
  else:
    present_counts(data, options.daily_count)
  return 0


if __name__ == '__main__':
  sys.exit(main())
