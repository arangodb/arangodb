# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes representing the monitoring interface for tasks or devices."""


import base64
import json
import logging

from googleapiclient import discovery
from oauth2client import gce
from oauth2client.client import GoogleCredentials
from oauth2client.file import Storage
import httplib2

from infra_libs import httplib2_utils
from infra_libs.ts_mon.common import http_metrics
from infra_libs.ts_mon.protos import metrics_pb2


# Special string that can be passed through as the credentials path to use the
# default Appengine or GCE service account.
APPENGINE_CREDENTIALS = ':appengine'
GCE_CREDENTIALS = ':gce'


class Monitor(object):
  """Abstract base class encapsulating the ability to collect and send metrics.

  This is a singleton class. There should only be one instance of a Monitor at
  a time. It will be created and initialized by process_argparse_options. It
  must exist in order for any metrics to be sent, although both Targets and
  Metrics may be initialized before the underlying Monitor. If it does not exist
  at the time that a Metric is sent, an exception will be raised.
  """
  @staticmethod
  def _wrap_proto(data):
    """Normalize MetricsData, list(MetricsData), and MetricsCollection.

    Args:
      input: A MetricsData, list of MetricsData, or a MetricsCollection.

    Returns:
      A MetricsCollection with the appropriate data attribute set.
    """
    if isinstance(data, metrics_pb2.MetricsCollection):
      ret = data
    elif isinstance(data, list):
      ret = metrics_pb2.MetricsCollection(data=data)
    else:
      ret = metrics_pb2.MetricsCollection(data=[data])
    return ret

  def send(self, metric_pb):
    raise NotImplementedError()


class PubSubMonitor(Monitor):
  """Class which publishes metrics to a Cloud Pub/Sub topic."""

  _SCOPES = [
      'https://www.googleapis.com/auth/pubsub',
  ]

  TIMEOUT = 10  # seconds

  def _load_credentials(self, credentials_file_path):
    if credentials_file_path == GCE_CREDENTIALS:
      return gce.AppAssertionCredentials(self._SCOPES)
    if credentials_file_path == APPENGINE_CREDENTIALS:  # pragma: no cover
      # This import doesn't work outside appengine, so delay it until it's used.
      from oauth2client import appengine
      from google.appengine.api import app_identity
      logging.info('Initializing with service account %s',
                   app_identity.get_service_account_name())
      return appengine.AppAssertionCredentials(self._SCOPES)

    with open(credentials_file_path, 'r') as credentials_file:
      credentials_json = json.load(credentials_file)
    if credentials_json.get('type', None):
      credentials = GoogleCredentials.from_stream(credentials_file_path)
      credentials = credentials.create_scoped(self._SCOPES)
      return credentials
    return Storage(credentials_file_path).get()

  def _initialize(self):
    creds = self._load_credentials(self._credsfile)
    creds.authorize(self._http)
    self._api = discovery.build('pubsub', 'v1', http=self._http)

  def _update_init_metrics(self, status):
    if not self._use_instrumented_http:
      return
    fields = {'name': 'acq-mon-api-pubsub',
              'client': 'discovery',
              'status': status}
    http_metrics.response_status.increment(fields=fields)

  def _check_initialize(self):
    if self._api:
      return True
    try:
      self._initialize()
    except EnvironmentError:
      logging.exception('PubSubMonitor._initialize failed')
      self._api = None
      self._update_init_metrics(http_metrics.STATUS_ERROR)
      return False

    self._update_init_metrics(http_metrics.STATUS_OK)
    return True

  def __init__(self, credsfile, project, topic, use_instrumented_http=True):
    """Process monitoring related command line flags and initialize api.

    Args:
      credsfile (str): path to the credentials json file
      project (str): the name of the Pub/Sub project to publish to.
      topic (str): the name of the Pub/Sub topic to publish to.
      use_instrumented_http (bool): whether to record monitoring metrics for
          HTTP requests made to the pubsub API.
    """
    self._api = None
    self._use_instrumented_http = use_instrumented_http
    if use_instrumented_http:
      self._http = httplib2_utils.InstrumentedHttp(
          'acq-mon-api-pubsub', timeout=self.TIMEOUT)
    else:
      self._http = httplib2.Http(timeout=self.TIMEOUT)
    self._credsfile = credsfile
    self._topic = 'projects/%s/topics/%s' % (project, topic)
    self._check_initialize()

  def send(self, metric_pb):
    """Send a metric proto to the monitoring api.

    Args:
      metric_pb (MetricsData or MetricsCollection): the metric protobuf to send
    """
    if not self._check_initialize():
      return
    proto = self._wrap_proto(metric_pb)
    logging.debug('ts_mon: sending %d metrics to PubSub', len(proto.data))
    body = {
        'messages': [
          {'data': base64.b64encode(proto.SerializeToString())},
        ],
    }
    self._api.projects().topics().publish(
        topic=self._topic,
        body=body).execute(num_retries=5)


class DebugMonitor(Monitor):
  """Class which writes metrics to logs or a local file for debugging."""
  def __init__(self, filepath=None):
    if filepath is None:
      self._fh = None
    else:
      self._fh = open(filepath, 'a')

  def send(self, metric_pb):
    text = str(self._wrap_proto(metric_pb))
    logging.info('Flushing monitoring metrics:\n%s', text)
    if self._fh is not None:
      self._fh.write(text + '\n\n')
      self._fh.flush()


class NullMonitor(Monitor):
  """Class that doesn't send metrics anywhere."""
  def send(self, metric_pb):
    pass
