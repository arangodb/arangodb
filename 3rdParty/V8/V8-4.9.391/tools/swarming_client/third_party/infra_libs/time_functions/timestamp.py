# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Provides common timestamp functions."""

import datetime
import pytz


def utctimestamp(dt):
  """Converts a datetime object into a floating point timestamp since the epoch.

  dt is the datetime to convert. If dt is a naive (non-tz-aware) object, it
     will implicitly be treated as UTC.
  """
  epoch = datetime.datetime.utcfromtimestamp(0).replace(tzinfo=pytz.UTC)
  # This check is from http://stackoverflow.com/a/27596917/3984761.
  if dt.tzinfo is None or dt.tzinfo.utcoffset(dt) is None:
    dt = dt.replace(tzinfo=pytz.UTC)
  return (dt - epoch).total_seconds()


def utcnow_ts():  # pragma: no cover
  """Returns the floating point number of seconds since the UTC epoch."""
  return utctimestamp(datetime.datetime.utcnow())
