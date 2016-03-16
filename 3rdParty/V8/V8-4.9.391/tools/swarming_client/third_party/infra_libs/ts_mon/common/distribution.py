# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import bisect
import collections


class Bucketer(object):
  """Bucketing function for histograms recorded by the Distribution class."""

  def __init__(self, width, growth_factor, num_finite_buckets):
    """The bucket sizes are controlled by width and growth_factor, and the total
    number of buckets is set by num_finite_buckets:

    Args:
      width: fixed size of each bucket.
      growth_factor: if non-zero, the size of each bucket increases by another
          multiplicative factor of this factor (see lower bound formula below).
      num_finite_buckets: the number of finite buckets.  There are two
          additional buckets - an underflow and an overflow bucket - that have
          lower and upper bounds of Infinity.

    Specify a width for fixed-size buckets or specify a growth_factor for bucket
    sizes that follow a geometric progression.  Specifying both is valid as
    well::

      lower bound of bucket i = width * i + growth_factor ^ (i - 1)
    """

    if num_finite_buckets < 0:
      raise ValueError('num_finite_buckets must be >= 0 (was %d)' %
          num_finite_buckets)

    self.width = width
    self.growth_factor = growth_factor
    self.num_finite_buckets = num_finite_buckets
    self.total_buckets = num_finite_buckets + 2
    self.underflow_bucket = 0
    self.overflow_bucket = self.total_buckets - 1

    self._lower_bounds = list(self._generate_lower_bounds())

  def _generate_lower_bounds(self):
    yield float('-Inf')
    yield 0

    previous = 0
    for i in xrange(self.num_finite_buckets):
      lower_bound = self.width * (i + 1)
      if self.growth_factor != 0:
        lower_bound += self.growth_factor ** i

      if lower_bound <= previous:
        raise ValueError('bucket boundaries must be monotonically increasing')
      yield lower_bound
      previous = lower_bound

  def bucket_for_value(self, value):
    """Returns the index of the bucket that this value belongs to."""

    # bisect.bisect_left is wrong because the buckets are of [lower, upper) form
    return bisect.bisect(self._lower_bounds, value) - 1

  def bucket_boundaries(self, bucket):
    """Returns a tuple that is the [lower, upper) bounds of this bucket.

    The lower bound of the first bucket is -Infinity, and the upper bound of the
    last bucket is +Infinity.
    """

    if bucket < 0 or bucket >= self.total_buckets:
      raise IndexError('bucket %d out of range' % bucket)
    if bucket == self.total_buckets - 1:
      return (self._lower_bounds[bucket], float('Inf'))
    return (self._lower_bounds[bucket], self._lower_bounds[bucket + 1])

  def all_bucket_boundaries(self):
    """Generator that produces the [lower, upper) bounds of all buckets.

    This is equivalent to calling::

      (b.bucket_boundaries(i) for i in xrange(b.total_buckets))

    but is more efficient.
    """

    lower = self._lower_bounds[0]
    for i in xrange(1, self.total_buckets):
      upper = self._lower_bounds[i]
      yield (lower, upper)
      lower = upper

    yield (lower, float('Inf'))


def FixedWidthBucketer(width, num_finite_buckets=100):
  """Convenience function that returns a fixed width Bucketer."""
  return Bucketer(width=width, growth_factor=0.0,
      num_finite_buckets=num_finite_buckets)


def GeometricBucketer(growth_factor=10**0.2, num_finite_buckets=100):
  """Convenience function that returns a geometric progression Bucketer."""
  return Bucketer(width=0, growth_factor=growth_factor,
      num_finite_buckets=num_finite_buckets)


class Distribution(object):
  """Holds a histogram distribution.

  Buckets are chosen for values by the provided Bucketer.
  """

  def __init__(self, bucketer):
    self.bucketer = bucketer
    self.sum = 0
    self.count = 0
    self.buckets = collections.defaultdict(int)

  def add(self, value):
    self.buckets[self.bucketer.bucket_for_value(value)] += 1
    self.sum += value
    self.count += 1
