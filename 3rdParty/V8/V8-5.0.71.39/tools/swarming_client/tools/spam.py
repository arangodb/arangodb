#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""spam.py spams stdout for load testing the stdout handler.

To use on the server, use:
  export ISOLATE=https://your-server.appspot.com
  export SWARMING=https://your-server.appspot.com
  ../isolate.py archive -I $ISOLATE -i spam.isolate -s spam.isolated
  # Where Linux can also be Mac or Windows.
  ../swarming.py run -I $ISOLATE -S $SWARMING spam.isolated -d os Linux \
--priority 10 -- --duration 600 --sleep 0.5 --size 1024

./run_on_bot.py can also be used to trigger systematically on all bots.
 """

import optparse
import sys
import time


def main():
  parser = optparse.OptionParser(
      description=sys.modules[__name__].__doc__, usage='%prog [options]')
  parser.format_description = lambda _: parser.description
  parser.add_option(
      '--duration', type='float', default=5., help='Duration in seconds')
  parser.add_option(
      '--sleep', type='float', default=1.,
      help='Sleep in seconds between burst')
  parser.add_option(
      '--size', type='int', default=10, help='Data written at each burst')
  options, args = parser.parse_args()
  if args:
    parser.error('Unknown args: %s' % args)
  if options.duration <= 0:
    parser.error('Invalid --duration')
  if options.sleep < 0:
    parser.error('Invalid --sleep')
  if options.size < 1:
    parser.error('Invalid --size')

  print('Duration: %gs' % options.duration)
  print('Sleep: %gs' % options.sleep)
  print('Bursts size: %d' % options.size)
  start = time.time()
  end = start + options.duration
  index = 0
  while True:
    sys.stdout.write(str(index) * (options.size - 1))
    sys.stdout.write('\n')
    if time.time() > end:
      break
    time.sleep(options.sleep)
    index = (index + 1) % 10
  return 0


if __name__ == '__main__':
  sys.exit(main())
