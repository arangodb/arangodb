# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper classes that make it easier to instrument code for monitoring."""


class ScopedIncrementCounter(object):
  """Increment a counter when the wrapped code exits.

  The counter will be given a 'status' = 'success' or 'failure' label whose
  value will be set to depending on whether the wrapped code threw an exception.

  Example:

    mycounter = Counter('foo/stuff_done')
    with ScopedIncrementCounter(mycounter):
      DoStuff()

  To set a custom status label and status value:

    mycounter = Counter('foo/http_requests')
    with ScopedIncrementCounter(mycounter, 'response_code') as sc:
      status = MakeHttpRequest()
      sc.set_status(status)  # This custom status now won't be overwritten if
                             # the code later raises an exception.
  """

  def __init__(self, counter, label='status', success_value='success',
               failure_value='failure'):
    self.counter = counter
    self.label = label
    self.success_value = success_value
    self.failure_value = failure_value
    self.status = None

  def set_failure(self):
    self.set_status(self.failure_value)

  def set_status(self, status):
    self.status = status

  def __enter__(self):
    self.status = None
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    if self.status is None:
      if exc_type is None:
        self.status = self.success_value
      else:
        self.status = self.failure_value
    self.counter.increment({self.label: self.status})
