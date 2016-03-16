# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Provides functions for parsing and outputting Zulu time."""

import datetime
import pytz

from infra_libs.time_functions import timestamp


def parse_zulu_time(string):
  """Parses a Zulu time string, returning None if unparseable."""

  # Ugh https://bugs.python.org/issue19475.
  zulu_format = "%Y-%m-%dT%H:%M:%S"
  if '.' in string:
    zulu_format += ".%f"
  zulu_format += "Z"
  try:
    return datetime.datetime.strptime(string, zulu_format)
  except ValueError:
    return None


def parse_zulu_ts(string):
  """Parses Zulu time and converts into a timestamp or None."""
  zuluparse = parse_zulu_time(string)
  if zuluparse is None:
    return None
  return timestamp.utctimestamp(zuluparse)


def to_zulu_string(dt):
  """Returns a Zulu time string from a datetime.

  Assumes naive datetime objects are in UTC.
  Ensures the output always has a floating-point number of seconds.
  """

  # Assume non-tz-aware datetimes are in UTC.
  if dt.tzinfo is None or dt.tzinfo.utcoffset(dt) is None:
    dt = dt.replace(tzinfo=pytz.UTC)
  else:
    print dt

  # Convert datetime into UTC.
  isodate = dt.astimezone(pytz.UTC).isoformat().split('+')[0]

  # Add fractional seconds if not present.
  if '.' not in isodate:
    isodate += '.0'

  return isodate + 'Z'
