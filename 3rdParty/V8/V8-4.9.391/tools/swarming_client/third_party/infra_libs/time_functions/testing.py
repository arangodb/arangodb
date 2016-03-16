# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Provides functions to mock current time in tests."""

import datetime
import functools
import mock
import pytz
import tzlocal


def mock_datetime_utc(*dec_args, **dec_kwargs):
  """Overrides built-in datetime and date classes to always return a given time.

  Args:
    Same arguments as datetime.datetime accepts to mock UTC time.
  
  Example usage:
    @mock_datetime_utc(2015, 10, 11, 20, 0, 0)
    def my_test(self):
      hour_ago_utc = datetime.datetime.utcnow() - datetime.timedelta(hours=1)
      self.assertEqual(hour_ago_utc, datetime.datetime(2015, 10, 11, 19, 0, 0))

  Note that if you are using now() and today() methods, you should also use
  mock_timezone decorator to have consistent test results across timezones:

    @mock_timezone('US/Pacific')
    @mock_datetime_utc(2015, 10, 11, 20, 0, 0)
    def my_test(self):
      local_dt = datetime.datetime.now()
      self.assertEqual(local_dt, datetime.datetime(2015, 10, 11, 12, 0, 0))
  """
  # We record original values currently stored in the datetime.datetime and
  # datetime.date here. Note that they are no necessarily vanilla Python types
  # and can already be mock classes - this can happen if nested mocking is used.
  original_datetime = datetime.datetime
  original_date = datetime.date

  # Our metaclass must be derived from the parent class metaclass, but if the
  # parent class doesn't have one, we use 'type' type.
  class MockDateTimeMeta(original_datetime.__dict__.get('__metaclass__', type)):
    @classmethod
    def __instancecheck__(cls, instance):
      return isinstance(instance, original_datetime)

  class _MockDateTime(original_datetime):
    __metaclass__ = MockDateTimeMeta
    mock_utcnow = original_datetime(*dec_args, **dec_kwargs)
  
    @classmethod
    def utcnow(cls):
      return cls.mock_utcnow
  
    @classmethod
    def now(cls, tz=None):
      if not tz:
        tz = tzlocal.get_localzone()
      tzaware_utcnow = pytz.utc.localize(cls.mock_utcnow)
      return tz.normalize(tzaware_utcnow.astimezone(tz)).replace(tzinfo=None)
  
    @classmethod
    def today(cls):
      return cls.now().date()

    @classmethod
    def fromtimestamp(cls, timestamp, tz=None):
      if not tz:
        tz = tzlocal.get_localzone()
      tzaware_dt = pytz.utc.localize(cls.utcfromtimestamp(timestamp))
      return tz.normalize(tzaware_dt.astimezone(tz)).replace(tzinfo=None)
  
  # Our metaclass must be derived from the parent class metaclass, but if the
  # parent class doesn't have one, we use 'type' type.
  class MockDateMeta(original_date.__dict__.get('__metaclass__', type)):
    @classmethod
    def __instancecheck__(cls, instance):
      return isinstance(instance, original_date)

  class _MockDate(original_date):
    __metaclass__ = MockDateMeta

    @classmethod
    def today(cls):
      return _MockDateTime.today()

    @classmethod
    def fromtimestamp(cls, timestamp, tz=None):
      return _MockDateTime.fromtimestamp(timestamp, tz).date()

  def decorator(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
      with mock.patch('datetime.datetime', _MockDateTime):
        with mock.patch('datetime.date', _MockDate):
          return func(*args, **kwargs)
    return wrapper
  return decorator


def mock_timezone(tzname):
  """Mocks tzlocal.get_localzone method to always return a given timezone.

  This should be used in combination with mock_datetime_utc in order to achieve
  consistent test results accross timezones if datetime.now, datetime.today or
  date.today functions are used.
  
  Args:
    tzname: Name of the timezone to be used (as passed to pytz.timezone).
  """
  # TODO(sergiyb): Also mock other common libraries, e.g. time, pytz.reference.
  def decorator(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
      with mock.patch('tzlocal.get_localzone', lambda: pytz.timezone(tzname)):
        return func(*args, **kwargs)
    return wrapper
  return decorator
