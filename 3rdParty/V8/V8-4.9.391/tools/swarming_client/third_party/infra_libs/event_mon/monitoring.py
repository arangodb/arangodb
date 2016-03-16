# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from google.protobuf.message import DecodeError
from infra_libs.event_mon.protos.chrome_infra_log_pb2 import (
  ChromeInfraEvent, ServiceEvent, BuildEvent)
from infra_libs.event_mon.protos.goma_stats_pb2 import GomaStats
from infra_libs.event_mon.protos.log_request_lite_pb2 import LogRequestLite
from infra_libs.event_mon import config, router


# These constants are part of the API.
EVENT_TYPES = ('START', 'STOP', 'UPDATE', 'CURRENT_VERSION', 'CRASH')
BUILD_EVENT_TYPES = ('SCHEDULER', 'BUILD', 'STEP')
BUILD_RESULTS = ('UNKNOWN', 'SUCCESS', 'FAILURE', 'INFRA_FAILURE',
                 'WARNING', 'SKIPPED', 'RETRY')
TIMESTAMP_KINDS = ('UNKNOWN', 'POINT', 'BEGIN', 'END')
GOMA_ERROR_TYPES = ('GOMA_ERROR_OK', 'GOMA_ERROR_UNKNOWN', 'GOMA_ERROR_CRASHED',
                    'GOMA_ERROR_LOG_FATAL')

# Maximum size of stack trace sent in an event, in characters.
STACK_TRACE_MAX_SIZE = 1000


class Event(object):
  """Wraps the event proto with the necessary boilerplate code."""

  def __init__(self, timestamp_kind=None,
               event_timestamp_ms=None, service_name=None):
    """
    Args:
      timestamp_kind (string): 'POINT', 'START' or 'STOP'.
      event_timestamp_ms (int or float): time of the event in milliseconds
         from Unix epoch. Default: now.
      service_name (string): name of the monitored service.
    """
    self._timestamp_ms = event_timestamp_ms
    self._event =  _get_chrome_infra_event(
        timestamp_kind, service_name=service_name)

  @property
  def is_null(self):
    return self.proto is None

  @staticmethod
  def null():
    """Create an "null" Event, without the proto.

    Null event's send() method will fail (return False). This is useful for
    returning a consistent object type from helper functions even in the
    case of failure.
    """
    event = Event()
    event._event = None
    return event

  @property
  def proto(self):
    return self._event

  def log_event(self):
    if self.is_null:
      return None
    return _get_log_event_lite(
        self.proto, event_timestamp=self._timestamp_ms)

  def send(self):
    if self.proto is None:
      return False
    return config._router.push_event(self.log_event())


def _get_chrome_infra_event(timestamp_kind, service_name=None):
  """Compute a basic event.

  Validates the inputs and returns a pre-filled ChromeInfraEvent or
  None if any check failed.

  The proto is filled using values provided in setup_monitoring() at
  initialization time, and args.

  Args:
    timestamp_kind (string): any of ('POINT', 'BEGIN', 'END').

  Returns:
    event (chrome_infra_log_pb2.ChromeInfraEvent):
  """
  # Testing for None because we want an error message when timestamp_kind == ''.
  if timestamp_kind is not None and timestamp_kind not in TIMESTAMP_KINDS:
    logging.error('Invalid value for timestamp_kind: %s', timestamp_kind)
    return None

  # We must accept unicode here.
  if service_name is not None and not isinstance(service_name, basestring):
    logging.error('Invalid type for service_name: %s', type(service_name))
    return None

  event = ChromeInfraEvent()
  event.CopyFrom(config._cache['default_event'])

  if timestamp_kind:
    event.timestamp_kind = ChromeInfraEvent.TimestampKind.Value(timestamp_kind)
  if service_name:
    event.event_source.service_name = service_name

  return event


def _get_log_event_lite(chrome_infra_event, event_timestamp=None):
  """Wraps a ChromeInfraEvent into a LogEventLite.

  Args:
    event_timestamp (int or float): timestamp of when the event happened
      as a number of milliseconds since the epoch. If None, the current time
      is used.

  Returns:
    log_event (log_request_lite_pb2.LogRequestLite.LogEventLite):
  """
  if not isinstance(event_timestamp, (int, float, None.__class__ )):
    logging.error('Invalid type for event_timestamp. Needs a number, got %s',
                  type(event_timestamp))
    return None

  log_event = LogRequestLite.LogEventLite()
  log_event.event_time_ms = int(event_timestamp or router.time_ms())
  log_event.source_extension = chrome_infra_event.SerializeToString()
  return log_event


def _get_service_event(event_type,
                       timestamp_kind=None,
                       event_timestamp=None,
                       code_version=None,
                       stack_trace=None,
                       service_name=None):
  """Compute a ChromeInfraEvent filled with a ServiceEvent.
  Arguments are identical to those in send_service_event(), please refer
  to this docstring.

  Returns:
    event (Event): can be a "null" Event if there is a major processing issue.
  """
  if event_type not in EVENT_TYPES:
    logging.error('Invalid value for event_type: %s', event_type)
    return Event.null()

  if timestamp_kind is None:
    timestamp_kind = 'POINT'
    if event_type == 'START':
      timestamp_kind = 'BEGIN'
    elif event_type == 'STOP':
      timestamp_kind = 'END'
    elif event_type == 'CRASH':
      timestamp_kind = 'END'

  event_wrapper = Event(timestamp_kind, event_timestamp, service_name)
  if event_wrapper.is_null:
    return event_wrapper

  event = event_wrapper.proto

  event.service_event.type = getattr(ServiceEvent, event_type)

  if code_version is None:
    code_version = ()
  if not isinstance(code_version, (tuple, list)):
    logging.error('Invalid type provided to code_version argument in '
                  '_get_service_event. Please fix the calling code. '
                  'Type provided: %s, expected list, tuple or None.',
                  type(code_version))
    code_version = ()

  for version_d in code_version:
    try:
      if 'source_url' not in version_d:
        logging.error('source_url missing in %s', version_d)
        continue

      version = event.service_event.code_version.add()
      version.source_url = version_d['source_url']
      if 'revision' in version_d:
        # Rely on the url to switch between svn and git because an
        # abbreviated sha1 can sometimes be confused with an int.
        if version.source_url.startswith('svn://'):
          version.svn_revision = int(version_d['revision'])
        else:
          version.git_hash = version_d['revision']

      if 'version' in version_d:
        version.version = version_d['version']
      if 'dirty' in version_d:
        version.dirty = version_d['dirty']

    except TypeError:
      logging.exception('Invalid type provided to code_version argument in '
                        '_get_service_event. Please fix the calling code.')
      continue

  if isinstance(stack_trace, basestring):
    if event_type != 'CRASH':
      logging.error('stack_trace provide for an event different from CRASH.'
                    ' Got: %s', event_type)
    event.service_event.stack_trace = stack_trace[-STACK_TRACE_MAX_SIZE:]
  else:
    if stack_trace is not None:
      logging.error('stack_trace should be a string, got %s',
                    stack_trace.__class__.__name__)

  return event_wrapper


def send_service_event(event_type,
                       timestamp_kind=None,
                       event_timestamp=None,
                       code_version=(),
                       stack_trace=None):
  """Send service event.

  Args:
    event_type (string): any name of enum ServiceEvent.ServiceEventType.
      ('START', 'STOP', 'UPDATE', 'CURRENT_VERSION', 'CRASH')

  Keyword Args:
    timestamp_kind (string): any of ('POINT', 'BEGIN', 'END').

    event_timestamp (int or float): timestamp of when the event happened
      as a number of milliseconds since the epoch. If not provided, the
      current time is used.

    code_version (list/tuple of dict or None): required keys are
        'source_url' -> full url to the repository
        'revision' -> (string) git sha1 or svn revision number.
      optional keys are
        'dirty' -> boolean. True if the local source tree has local
            modification.
        'version' -> manually-set version number (like 'v2.6.0')

    stack_trace (str): when event_type is 'CRASH', stack trace of the crash
      as a string. String is truncated to 1000 characters (the last ones
      are kept). Use traceback.format_exc() to get the stack trace from an
      exception handler.

  Returns:
    success (bool): False if some error happened.
  """
  return _get_service_event(event_type=event_type,
                            timestamp_kind=timestamp_kind,
                            service_name=None,
                            event_timestamp=event_timestamp,
                            code_version=code_version,
                            stack_trace=stack_trace).send()


def get_build_event(event_type,
                    hostname,
                    build_name,
                    build_number=None,
                    build_scheduling_time=None,
                    step_name=None,
                    step_number=None,
                    result=None,
                    extra_result_code=None,
                    timestamp_kind=None,
                    event_timestamp=None,
                    service_name=None,
                    goma_stats=None,
                    goma_error=None,
                    goma_crash_report_id=None):
  """Compute a ChromeInfraEvent filled with a BuildEvent.

  Arguments are identical to those in send_build_event(), please refer
  to this docstring.

  Returns:
    event (log_request_lite_pb2.LogRequestLite.LogEventLite): can be None
      if there is a major processing issue.
  """
  if event_type not in BUILD_EVENT_TYPES:
    logging.error('Invalid value for event_type: %s', event_type)
    return Event.null()

  event_wrapper = Event(timestamp_kind, event_timestamp,
                        service_name=service_name)
  if event_wrapper.is_null:
    return event_wrapper

  event = event_wrapper.proto
  event.build_event.type = BuildEvent.BuildEventType.Value(event_type)

  if hostname:
    event.build_event.host_name = hostname
  if not event.build_event.HasField('host_name'):
    logging.error('hostname must be provided, got %s', hostname)

  if build_name:
    event.build_event.build_name = build_name
  if not event.build_event.HasField('build_name'):
    logging.error('build_name must be provided, got %s', build_name)

  # 0 is a valid value for build_number
  if build_number is not None:
    event.build_event.build_number = build_number

  # 0 is not a valid scheduling time
  if build_scheduling_time:
    event.build_event.build_scheduling_time_ms = build_scheduling_time

  if event.build_event.HasField('build_number'):
    if event_type == 'SCHEDULER':
      logging.error('build_number should not be provided for a "SCHEDULER"'
                    ' type, got %s (drop or use BUILD or STEP type)',
                    build_number)

    if not event.build_event.HasField('build_scheduling_time_ms'):
      logging.error('build_number has been provided (%s), '
                    'build_scheduling_time was not. '
                    'Provide either both or none.',
                    event.build_event.build_number)
  else: # no 'build_number' field
    if event.build_event.HasField('build_scheduling_time_ms'):
      logging.error('build_number has not been provided, '
                    'build_scheduling_time was provided (%s). '
                    'Both must be present or missing.',
                    event.build_event.build_scheduling_time_ms)

  if step_name:
    event.build_event.step_name = step_name
  if step_number is not None:
    event.build_event.step_number = step_number

  if event.build_event.step_name:
    if event_type != 'STEP':
      logging.error('step_name should be provided only for type "STEP", '
                    'got %s', event_type)
    if not event.build_event.HasField('step_number'):
      logging.error('step_number was not provided, but got a value for '
                    'step_name (%s). Provide either both or none',
                    step_name)
    if (not event.build_event.HasField('build_number')
        and not event.build_event.HasField('build_scheduling_time_ms')):
      logging.error('build information must be provided when step '
                    'information is provided. Got nothing in build_name '
                    'and build_number')
  else:
    if event.build_event.HasField('step_number'):
      logging.error('step_number has been provided (%s), '
                    'step_name has not. '
                    'Both must be present or missing.',
                    event.build_event.step_number)

  # TODO(pgervais) remove this.
  # Hack to work around errors in the proto
  mapping = {'WARNINGS': 'WARNING', 'EXCEPTION': 'INFRA_FAILURE'}
  result = mapping.get(result, result)

  if result is not None:  # we want an error message if result==''.
    if result not in BUILD_RESULTS:
      logging.error('Invalid value for result: %s', result)
    else:
      event.build_event.result = getattr(BuildEvent, result)

      if event_type == 'SCHEDULER':
        logging.error('A result was provided for a "SCHEDULER" event type '
                      '(%s). This is only accepted for BUILD and TEST types.',
                      result)

  if isinstance(extra_result_code, basestring):
    extra_result_code = (extra_result_code, )
  if not isinstance(extra_result_code, (list, tuple)):
    if extra_result_code is not None:
      logging.error('extra_result_code must be a string or list of strings. '
                    'Got %s' % type(extra_result_code))
  else:
    non_strings = []
    extra_result_strings = []
    for s in extra_result_code:
      if not isinstance(s, basestring):
        non_strings.append(s)
      else:
        extra_result_strings.append(s)

    if non_strings:
      logging.error('some values provided to extra_result_code are not strings:'
                    ' %s' % str(non_strings))
    for s in extra_result_strings:
      event.build_event.extra_result_code.append(s)

  if goma_stats:
    if isinstance(goma_stats, GomaStats):
      event.build_event.goma_stats.MergeFrom(goma_stats)
    else:
      logging.error('expected goma_stats to be an instance of GomaStats, '
                    'got %s', type(goma_stats))
  if goma_error:
    if goma_stats:
      logging.error('Only one of goma_error and goma_stats can be provided. '
                    'Got %s and %s.', goma_error, goma_stats)
    event.build_event.goma_error = BuildEvent.GomaErrorType.Value(goma_error)
    if goma_crash_report_id:
      event.build_event.goma_crash_report_id = goma_crash_report_id
      if goma_error != 'GOMA_ERROR_CRASHED':
        logging.error('A crash report id (%s) was provided for GomaErrorType '
                      '(%s).  This is only accepted for GOMA_ERROR_CRASHED '
                      'type.', goma_crash_report_id, goma_error)

  return event_wrapper


def send_build_event(event_type,
                     hostname,
                     build_name,
                     build_number=None,
                     build_scheduling_time=None,
                     step_name=None,
                     step_number=None,
                     result=None,
                     extra_result_code=None,
                     timestamp_kind=None,
                     event_timestamp=None,
                     goma_stats=None,
                     goma_error=None,
                     goma_crash_report_id=None):
  """Send a ChromeInfraEvent filled with a BuildEvent

  Args:
    event_type (string): any name of enum BuildEvent.BuildEventType.
      (listed in infra_libs.event_mon.monitoring.BUILD_EVENT_TYPES)
    hostname (string): fqdn of the machine that is running the build / step.
      aka the bot name.
    build_name (string): name of the builder.

  Keyword args:
    build_number (int): as the name says.
    build_scheduling_time (int): timestamp telling when the build was
      scheduled. This is required when build_number is provided to make it
      possibly to distinguish two builds with the same build number.
    step_name (str): name of the step.
    step_number (int): rank of the step in the build. This is mandatory
      if step_name is provided, because step_name is not enough to tell the
      order.
    result (string): any name of enum BuildEvent.BuildResult.
      (listed in infra_libs.event_mon.monitoring.BUILD_RESULTS)
    extra_result_code (string or list of): arbitrary strings intended to provide
      more fine-grained information about the result.
    goma_stats (goma_stats_pb2.GomaStats): statistics output by the Goma proxy.
    goma_error (string): goma error type defined as GomaErrorType.
    goma_crash_report_id (string): id of goma crash report.

  Returns:
    success (bool): False if some error happened.
  """
  return get_build_event(event_type,
                         hostname,
                         build_name,
                         build_number=build_number,
                         build_scheduling_time=build_scheduling_time,
                         step_name=step_name,
                         step_number=step_number,
                         result=result,
                         extra_result_code=extra_result_code,
                         timestamp_kind=timestamp_kind,
                         event_timestamp=event_timestamp,
                         goma_stats=goma_stats,
                         goma_error=goma_error,
                         goma_crash_report_id=goma_crash_report_id).send()


def send_events(events):
  """Send several events at once to the endpoint.

  Args:
    events (iterable of Event): events to send

  Return:
    success (bool): True if data was successfully received by the endpoint.
  """
  return config._router.push_event(tuple(e.log_event() for e in events))
