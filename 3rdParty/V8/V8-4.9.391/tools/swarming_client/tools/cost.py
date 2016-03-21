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
MAJOR_OS, MAJOR_OS_ASAN, MINOR_OS, MINOR_OS_GPU = range(4)


def do_bucket(items, bucket_type):
  """Categorizes the tasks based on one of the bucket type defined above."""
  out = {}
  for task in items:
    if 'heartbeat:1' in task['tags']:
      # Skip heartbeats.
      continue

    is_asan = 'asan:1' in task['tags']
    os_tag = None
    gpu_tag = None
    for t in task['tags']:
      if t.startswith('os:'):
        os_tag = t[3:]
        if os_tag == 'Linux':
          # GPU tests still specify Linux.
          # TODO(maruel): Fix the recipe.
          os_tag = 'Ubuntu'
      elif t.startswith('gpu:'):
        gpu_tag = t[4:]

    if bucket_type in (MAJOR_OS, MAJOR_OS_ASAN):
      os_tag = os_tag.split('-')[0]
    tag = os_tag
    if bucket_type == MINOR_OS_GPU and gpu_tag and gpu_tag != 'none':
      tag += ' gpu:' + gpu_tag
    if bucket_type == MAJOR_OS_ASAN and is_asan:
      tag += ' ASan'
    out.setdefault(tag, []).append(task)

    # Also create global buckets for ASan.
    if bucket_type == MAJOR_OS_ASAN:
      tag = '(any OS) ASan' if is_asan else '(any OS) Not ASan'
      out.setdefault(tag, []).append(task)
  return out


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
  if not options.start:
    # Defaults to 25 hours ago.
    options.start = datetime.datetime.utcnow() - datetime.timedelta(
        seconds=25*60*60)
  else:
    options.start = parse_time_option(options.start)
  if not options.end:
    options.end = options.start + datetime.timedelta(days=1)
  else:
    options.end = parse_time_option(options.end)
  url = 'tasks/list?' + urllib.urlencode(
      {
        'start': int((options.start - _EPOCH).total_seconds()),
        'end': int((options.end - _EPOCH).total_seconds()),
      })
  cmd = [
    sys.executable, os.path.join(CLIENT_DIR, 'swarming.py'),
    'query',
    '-S', options.swarming,
    '--json', options.json,
    # Start chocking at 1b tasks. The chromium infrastructure is currently at
    # around 200k tasks/day.
    '--limit', '1000000000',
    '--progress',
    url,
  ]
  if options.verbose:
    cmd.append('--verbose')
    cmd.append('--verbose')
    cmd.append('--verbose')
  logging.info('%s', ' '.join(cmd))
  subprocess.check_call(cmd)
  print('')


def stats(tasks, show_cost):
  """Calculates and prints statistics about the tasks."""
  # Split tasks into 3 buckets.
  # - 'rn' means ran, not idempotent
  # - 'ri' means ran, idempotent
  # - 'dd' means deduplicated.
  rn = [
    i for i in tasks
    if not i.get('deduped_from') and not i.get('properties_hash')
  ]
  ri = [
    i for i in tasks if not i.get('deduped_from') and i.get('properties_hash')
  ]
  dd = [i for i in tasks if i.get('deduped_from')]

  # Note worthy results.
  failures = [i for i in tasks if i.get('failure')]
  internal_failures = [i for i in tasks if i.get('internal_failure')]
  two_tries = [
    i for i in tasks if i.get('try_number') == '2' and not i.get('deduped_from')
  ]
  # TODO(maruel): 'state'

  # Summations.
  duration_rn = sum(i.get('duration', 0.) for i in rn)
  duration_ri = sum(i.get('duration', 0.) for i in ri)
  duration_dd = sum(i.get('duration', 0.) for i in dd)
  duration_total = duration_rn + duration_ri + duration_dd
  cost_rn = sum(sum(i.get('costs_usd') or [0.]) for i in rn)
  cost_ri = sum(sum(i.get('costs_usd') or [0.]) for i in ri)
  cost_dd = sum(i.get('cost_saved_usd', 0.) for i in dd)
  cost_total = cost_rn + cost_ri + cost_dd
  pendings = [
    (parse_time(i['started_ts']) - parse_time(i['created_ts'])).total_seconds()
    for i in tasks if i.get('started_ts') and not i.get('deduped_from')
  ]
  pending_total = datetime.timedelta(seconds=round(sum(pendings)))
  pending_avg = datetime.timedelta(seconds=round(average(pendings)))
  pending_med = datetime.timedelta(seconds=round(median(pendings)))
  pending_p99 = datetime.timedelta(seconds=round(percentile(pendings, 99)))

  # Calculate percentages to understand load relativeness.
  percent_rn_nb_total = sp(len(rn), len(tasks))
  percent_ri_nb_total = sp(len(ri), len(tasks))
  percent_dd_nb_total = sp(len(dd), len(tasks))
  percent_dd_nb_rel = sp(len(dd), len(ri) + len(dd))
  percent_rn_duration_total = sp(duration_rn, duration_total)
  percent_ri_duration_total = sp(duration_ri, duration_total)
  percent_dd_duration_total = sp(duration_dd, duration_total)
  percent_dd_duration_rel = sp(duration_dd, duration_dd + duration_ri)
  percent_rn_cost_total = sp(cost_rn, cost_total)
  percent_ri_cost_total = sp(cost_ri, cost_total)
  percent_dd_cost_total = sp(cost_dd, cost_total)
  percent_dd_cost_rel = sp(cost_dd, cost_dd + cost_ri)
  reliability = 100. - sp(len(internal_failures), len(tasks))
  percent_failures = sp(len(failures), len(tasks))
  percent_two_tries = sp(len(two_tries), len(tasks))


  # Print results as a table.
  if rn:
    cost = '  %7.2f$ (%5.1f%%)' % (cost_rn, percent_rn_cost_total)
    print(
        '  %6d (%5.1f%%)  %18s (%5.1f%%)%s  '
        'Real tasks executed, not idempotent' % (
          len(rn), percent_rn_nb_total,
          seconds_to_timedelta(duration_rn), percent_rn_duration_total,
          cost if show_cost else ''))
  if ri:
    cost = '  %7.2f$ (%5.1f%%)' % (cost_ri, percent_ri_cost_total)
    print(
        '  %6d (%5.1f%%)  %18s (%5.1f%%)%s  '
        'Real tasks executed, idempotent' % (
          len(ri), percent_ri_nb_total,
          seconds_to_timedelta(duration_ri), percent_ri_duration_total,
          cost if show_cost else ''))
  if ri and rn:
    cost = '  %7.2f$      ' % (cost_rn + cost_ri)
    print(
        '  %6d           %18s         %s     '
        'Real tasks executed, all types' % (
          len(rn) + len(ri),
          seconds_to_timedelta(duration_rn + duration_ri),
          cost if show_cost else ''))
  if dd:
    cost = '  %7.2f$*(%5.1f%%)' % (cost_dd, percent_dd_cost_total)
    print(
        '  %6d*(%5.1f%%)  %18s*(%5.1f%%)%s  *Wasn\'t run, '
        'previous results reused' % (
          len(dd), percent_dd_nb_total,
          seconds_to_timedelta(duration_dd), percent_dd_duration_total,
          cost if show_cost else ''))
    cost = '           (%5.1f%%)' % (percent_dd_cost_rel)
    print(
        '         (%5.1f%%)                     (%5.1f%%)%s  '
        '  (relative to idempotent tasks only)' % (
          percent_dd_nb_rel, percent_dd_duration_rel,
          cost if show_cost else ''))
  if int(bool(rn)) + int(bool(ri)) + int(bool(dd)) > 1:
    cost = '           %7.2f$' % (cost_total)
    print(
        '  %6d           %18s%s           '
        'Total tasks' % (
          len(tasks), seconds_to_timedelta(duration_total),
          cost if show_cost else ''))
  print (
      '        Reliability:    %7g%%    Internal errors: %-4d' % (
        reliability, len(internal_failures)))
  print (
      '        Tasks failures: %-4d (%5.3f%%)' % (
        len(failures), percent_failures))
  print (
      '        Retried:        %-4d (%5.3f%%)  (Upgraded an internal failure '
      'to a successful task)' %
        (len(two_tries), percent_two_tries))
  print (
      '        Pending  Total: %13s    Avg: %7s    Median: %7s  P99%%: %7s' % (
        pending_total, pending_avg, pending_med, pending_p99))


def present_task_types(items, bucket_type, show_cost):
  cost = '    Usage Cost $USD' if show_cost else ''
  print('      Nb of Tasks              Total Duration%s' % cost)
  buckets = do_bucket(items, bucket_type)
  for index, (bucket, tasks) in enumerate(sorted(buckets.iteritems())):
    if index:
      print('')
    print('%s:' % (bucket))
    stats(tasks, show_cost)
  if buckets:
    print('')
  print('Global:')
  stats(items, show_cost)


def present_users(items):
  users = {}
  for task in items:
    user = ''
    for tag in task['tags']:
      if tag.startswith('user:'):
        if tag[5:]:
          user = tag[5:]
          break
      if tag == 'purpose:CI':
        user = 'CI'
        break
      if tag == 'heartbeat:1':
        user = 'heartbeat'
        break
    if user:
      users.setdefault(user, 0)
      users[user] += 1
  maxlen = max(len(i) for i in users)
  maxusers = 100
  for index, (name, tasks) in enumerate(
      sorted(users.iteritems(), key=lambda x: -x[1])):
    if index == maxusers:
      break
    print('%3d  %-*s: %d' % (index + 1, maxlen, name, tasks))


def main():
  parser = optparse.OptionParser(description=sys.modules['__main__'].__doc__)
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
      help='Swarming server to use')
  parser.add_option(
      '--start', help='Starting date in UTC; defaults to 25 hours ago')
  parser.add_option(
      '--end', help='End date in UTC; defaults to --start+1 day')
  parser.add_option(
      '--no-cost', action='store_false', dest='cost', default=True,
      help='Strip $ from display')
  parser.add_option(
      '--users', action='store_true', help='Display top users instead')
  parser.add_option(
      '--json', default='tasks.json',
      help='File containing raw data; default: %default')
  parser.add_option('-v', '--verbose', action='count', default=0)

  group = optparse.OptionGroup(parser, 'Grouping')
  group.add_option(
      '--major-os', action='store_const',
      dest='bucket', const=MAJOR_OS, default=MAJOR_OS,
      help='Classify by OS type, independent of OS version (default)')
  group.add_option(
      '--minor-os', action='store_const',
      dest='bucket', const=MINOR_OS,
      help='Classify by minor OS version')
  group.add_option(
      '--gpu', action='store_const',
      dest='bucket', const=MINOR_OS_GPU,
      help='Classify by minor OS version and GPU type when requested')
  group.add_option(
      '--asan', action='store_const',
      dest='bucket', const=MAJOR_OS_ASAN,
      help='Classify by major OS version and ASAN')
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
  first = items[-1]
  last = items[0]
  print(
      'From %s to %s  (%s)' % (
        first['created_ts'].split('.')[0],
        last['created_ts'].split('.')[0],
        parse_time(last['created_ts']) - parse_time(first['created_ts'])
        ))
  print('')

  if options.users:
    present_users(items)
  else:
    present_task_types(items, options.bucket, options.cost)
  return 0


if __name__ == '__main__':
  sys.exit(main())
