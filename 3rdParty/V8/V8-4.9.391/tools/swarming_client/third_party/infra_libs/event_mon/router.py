# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import random
import sys
import time

import httplib2

import infra_libs
from infra_libs.event_mon.protos.log_request_lite_pb2 import LogRequestLite
from infra_libs.event_mon.protos.chrome_infra_log_pb2 import ChromeInfraEvent


def time_ms():
  """Return current timestamp in milliseconds."""
  return int(1000 * time.time())


def backoff_time(attempt, retry_backoff=2., max_delay=30.):
  """Compute randomized exponential backoff time.

  Args:
    attempt (int): attempt number, starting at zero.

  Keyword Args:
    retry_backoff(float): backoff time on the first attempt.
    max_delay(float): maximum returned value.
  """
  delay = retry_backoff * (2 ** attempt)
  # Add +-25% of variation.
  delay += delay * ((random.random() - 0.5) / 2.)
  return min(delay, max_delay)


class _Router(object):
  """Route events to the right destination. Base class.

  This object is meant to be a singleton, and is not part of the API.
  Subclasses must implement _send_to_endpoint().

  Usage:
  router = _Router()
  event = ChromeInfraEvent.LogEventLite(...)
  ... fill in event ...
  router.push_event(event)
  """
  def push_event(self, log_events):
    """Enqueue event to push to the collection service.

    This method offers no guarantee on return that the event have been pushed
    externally, as some buffering can take place.

    Args:
      log_events (LogRequestLite.LogEventLite or list/tuple of): events.

    Returns:
      success (bool): False if an error happened. True means 'event accepted',
        but NOT 'event successfully pushed to the remote'.
    """
    if isinstance(log_events, LogRequestLite.LogEventLite):
      log_events = (log_events,)

    if not isinstance(log_events, (list, tuple)):
      logging.error('Invalid type for "event", should be LogEventLite or '
                    'list of. Got %s' % str(type(log_events)))
      return False

    request_p = LogRequestLite()
    request_p.log_source_name = 'CHROME_INFRA'
    request_p.log_event.extend(log_events)  # copies the protobuf
    # Sets the sending time here for safety, _send_to_endpoint should change it
    # if needed.
    request_p.request_time_ms = time_ms()
    return self._send_to_endpoint(request_p)

  def _send_to_endpoint(self, events):
    """Send a protobuf to wherever it should be sent.

    This method is called by push_event.
    If some computation is require, make sure to update events.request_time_ms
    right before sending.

    Args:
      events(LogRequestLite): protobuf to send.

    Returns:
      success(bool): whether POSTing/writing succeeded or not.
    """
    raise NotImplementedError('Please implement _send_to_endpoint().')


class _LocalFileRouter(_Router):
  def __init__(self, output_file, dry_run=False):
    """Initialize the router.

    Writes a serialized LogRequestLite protobuf in a local file. File is
    created/truncated before writing (no append).

    Args:
      output_file(str): path to file where to write the protobuf.

    Keyword Args:
      dry_run(bool): if True, the file is not written.
    """
    _Router.__init__(self)
    self.output_file = output_file
    self._dry_run = dry_run

  def _send_to_endpoint(self, events):
    try:
      if not os.path.isdir(os.path.dirname(self.output_file)):
        logging.error('File cannot be written, parent directory does '
                      'not exist: %s' % os.path.dirname(self.output_file))
      if self._dry_run:
        logging.info('Would have written in %s', self.output_file)
      else:
        with open(self.output_file, 'wb') as f:
          f.write(events.SerializeToString())  # pragma: no branch
    except Exception:
      logging.exception('Failed to write in file: %s', self.output_file)
      return False

    return True


class _TextStreamRouter(_Router):
  def __init__(self, stream=sys.stdout):
    """Initialize the router.

    Args:
      stream(File): where to write the output.
    """
    _Router.__init__(self)
    self.stream = stream

  def _send_to_endpoint(self, events):
    # Prints individual events because it's what we're usually interested in
    # in that case.
    infra_events = [str(ChromeInfraEvent.FromString(
      ev.source_extension)) for ev in events.log_event]
    try:
      self.stream.write('%s\n' % '\n'.join(infra_events))
    except Exception:
      logging.exception('Unable to write to provided stream')
      return False
    return True


class _LoggingStreamRouter(_Router):
  def __init__(self, severity=logging.INFO):
    """Initialize the router.

    Args:
      severity: severity of the messages for reporting events
    """
    _Router.__init__(self)
    self.severity = severity

  def _send_to_endpoint(self, events):
    try:
      for ev in events.log_event:
        ev_str = str(ChromeInfraEvent.FromString(ev.source_extension))
        logging.log(self.severity, 'Sending event_mon event:\n%s' % ev_str)
    except Exception:
      logging.exception('Unable to log the events')
      return False
    return True


class _HttpRouter(_Router):
  def __init__(self, cache, endpoint, timeout=10, try_num=3, retry_backoff=2.,
               dry_run=False, _sleep_fn=time.sleep):
    """Initialize the router.

    Args:
      cache(dict): This must be config._cache. Passed as a parameter to
        avoid a circular import.
      endpoint(str or None): None means 'dry run'. What would be sent is printed
        on stdout. If endpoint starts with 'https://' data is POSTed there.
        Otherwise it is interpreted as a file where to write serialized
        LogEventLite protos.
      try_num(int): max number of http requests send to the endpoint.
      retry_backoff(float): time in seconds before retrying posting to the
         endpoint. Randomized exponential backoff is applied on subsequent
         retries.
      dry_run(boolean): if True, no http request is sent. Instead a message is
         printed.
      _sleep_fn (function): function to wait specified number of seconds. This
        argument is provided for testing purposes.
    """
    HTTP_IDENTIFIER = 'event_mon'
    _Router.__init__(self)
    self.endpoint = endpoint
    self.try_num = try_num
    self.retry_backoff = retry_backoff
    self._cache = cache
    self._http = infra_libs.InstrumentedHttp(HTTP_IDENTIFIER, timeout=timeout)
    self._dry_run = dry_run
    self._sleep_fn = _sleep_fn

    # TODO(pgervais) pass this as parameters instead.
    if self._cache.get('service_account_creds'):
      try:
        logging.debug('Activating OAuth2 authentication.')
        self._http = infra_libs.get_authenticated_http(
          self._cache['service_account_creds'],
          service_accounts_creds_root=
              self._cache['service_accounts_creds_root'],
          scope='https://www.googleapis.com/auth/cclog',
          http_identifier=HTTP_IDENTIFIER,
          timeout=timeout
        )
      except IOError:
        logging.error('Unable to read credentials, requests will be '
                      'unauthenticated. File: %s',
                      self._cache.get('service_account_creds'))

  def _send_to_endpoint(self, events):
    """Send protobuf to endpoint

    Args:
      events(LogRequestLite): the protobuf to send.

    Returns:
      success(bool): whether POSTing/writing succeeded or not.
    """
    if not self.endpoint.startswith('https://'):
      logging.error("Received invalid https endpoint: %s", self.endpoint)
      return False

    logging.info('event_mon: POSTing events to %s', self.endpoint)

    attempt = 0  # silencing pylint
    for attempt in xrange(self.try_num - 1):  # pragma: no branch
      # (re)set this time at the very last moment
      events.request_time_ms = time_ms()
      response = None
      try:
        if self._dry_run:
          logging.info('Http requests disabled. Not sending anything')
        else:  # pragma: no cover
          response, _ = self._http.request(
            uri=self.endpoint,
            method='POST',
            headers={'Content-Type': 'application/octet-stream'},
            body=events.SerializeToString()
          )

        if self._dry_run or response.status == 200:
          return True
      except Exception:
        logging.exception('exception when POSTing data')

      if response:
        logging.error('failed to POST data to %s Status: %d (attempt %d)',
                      self.endpoint, response.status, attempt)

      if attempt == 0:
        logging.error('data: %s', str(events)[:200])

      self._sleep_fn(backoff_time(attempt, retry_backoff=self.retry_backoff))

    logging.error('failed to POST data after %d attempts, giving up.',
                  attempt+1)
    return False
