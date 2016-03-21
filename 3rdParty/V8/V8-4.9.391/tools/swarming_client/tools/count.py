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
import Queue
import threading
import sys
import urllib


CLIENT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, CLIENT_DIR)

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


def fetch_data(swarming, start, end, state, tags):
  """Fetches data from swarming and returns it."""
  # Split the work in days. That's a lot of requests to do.
  queue = Queue.Queue()
  threads = []
  def run(start, cmd):
    data = json.loads(subprocess.check_output(cmd))
    queue.put((start, int(data['count'])))

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
      'query', '-S', swarming, 'tasks/count?' + urllib.urlencode(data),
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


def present(items, daily_count):
  months = {}
  for day, count in sorted(items.iteritems()):
    month = day.rsplit('-', 1)[0]
    months.setdefault(month, 0)
    months[month] += count

  years = {}
  for month, count in months.iteritems():
    year = month.rsplit('-', 1)[0]
    years.setdefault(year, 0)
    years[year] += count
  total = sum(months.itervalues())
  maxlen = len(str(total))

  if daily_count:
    for day, count in sorted(items.iteritems()):
      print('%s: %*d' % (day, maxlen, count))

  if len(items) > 1:
    for month, count in sorted(months.iteritems()):
      print('%s   : %*d' % (month, maxlen, count))
  if len(month) > 1:
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
    'ALL')


def main():
  parser = optparse.OptionParser(description=sys.modules['__main__'].__doc__)
  tomorrow = datetime.datetime.utcnow().date() + datetime.timedelta(days=1)
  year = datetime.datetime(tomorrow.year, 1, 1)
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
      help='Swarming server to use')
  parser.add_option(
      '--start', default=year.strftime('%Y-%m-%d'),
      help='Starting date in UTC; defaults to start of year: %default')
  parser.add_option(
      '--end', default=tomorrow.strftime('%Y-%m-%d'),
      help='End date in UTC; defaults to tomorrow: %default')
  parser.add_option(
      '--state', default='ALL', type='choice', choices=STATES,
      help='State to filter on')
  parser.add_option(
      '--tags', action='append', default=[], help='Tags to filter on')
  parser.add_option(
      '--daily-count', action='store_true',
      help='Show the daily count in raw number instead of histogram')
  parser.add_option(
      '--json', default='counts.json',
      help='File containing raw data; default: %default')
  parser.add_option('-v', '--verbose', action='count', default=0)
  options, args = parser.parse_args()

  if args:
    parser.error('Unsupported argument %s' % args)
  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)
  start = parse_time_option(options.start)
  end = parse_time_option(options.end)
  print('From %s to %s' % (start, end))
  if options.swarming:
    data = fetch_data(options.swarming, start, end, options.state, options.tags)
    with open(options.json, 'wb') as f:
      json.dump(data, f)
  elif not os.path.isfile(options.json):
    parser.error('--swarming is required.')
  else:
    with open(options.json, 'rb') as f:
      data = json.load(f)

  print('')
  present(data, options.daily_count)
  return 0


if __name__ == '__main__':
  sys.exit(main())
