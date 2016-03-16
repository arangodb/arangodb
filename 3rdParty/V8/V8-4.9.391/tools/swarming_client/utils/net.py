# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Classes and functions for generic network communication over HTTP."""

import cookielib
import cStringIO as StringIO
import datetime
import httplib
import itertools
import json
import logging
import math
import os
import random
import re
import socket
import ssl
import threading
import time
import urllib
import urlparse

from third_party import requests
from third_party.requests import adapters
from third_party.requests import structures

from utils import oauth
from utils import tools


# TODO(vadimsh): Remove this once we don't have to support python 2.6 anymore.
def monkey_patch_httplib():
  """Patch httplib.HTTPConnection to have '_tunnel_host' attribute.

  'requests' library (>= v2) accesses 'HTTPConnection._tunnel_host' attribute
  added only in python 2.6.3. This function patches HTTPConnection to have it
  on python 2.6.2 as well.
  """
  conn = httplib.HTTPConnection('example.com')
  if not hasattr(conn, '_tunnel_host'):
    httplib.HTTPConnection._tunnel_host = None
monkey_patch_httplib()


# Default maximum number of attempts to trying opening a url before aborting.
URL_OPEN_MAX_ATTEMPTS = 30

# Default timeout when retrying.
URL_OPEN_TIMEOUT = 6*60.

# Default timeout when reading from open HTTP connection.
URL_READ_TIMEOUT = 60

# Content type for url encoded POST body.
URL_ENCODED_FORM_CONTENT_TYPE = 'application/x-www-form-urlencoded'
# Content type for JSON body.
JSON_CONTENT_TYPE = 'application/json; charset=UTF-8'
# Default content type for POST body.
DEFAULT_CONTENT_TYPE = URL_ENCODED_FORM_CONTENT_TYPE

# Content type -> function that encodes a request body.
CONTENT_ENCODERS = {
  URL_ENCODED_FORM_CONTENT_TYPE:
    urllib.urlencode,
  JSON_CONTENT_TYPE:
    lambda x: json.dumps(x, sort_keys=True, separators=(',', ':')),
}


# Google Storage URL regular expression.
GS_STORAGE_HOST_URL_RE = re.compile(r'https://.*\.storage\.googleapis\.com')

# Global (for now) map: server URL (http://example.com) -> HttpService instance.
# Used by get_http_service to cache HttpService instances.
_http_services = {}
_http_services_lock = threading.Lock()

# This lock ensures that user won't be confused with multiple concurrent
# login prompts.
_auth_lock = threading.Lock()

# Set in 'set_oauth_config'. If 'set_oauth_config' is not called before the
# first request, will be set to oauth.make_oauth_config().
_auth_config = None

# A class to use to send HTTP requests. Can be changed by 'set_engine_class'.
# Default is RequestsLibEngine.
_request_engine_cls = None


class NetError(IOError):
  """Generic network related error."""

  def __init__(self, inner_exc=None):
    super(NetError, self).__init__(str(inner_exc or self.__doc__))
    self.inner_exc = inner_exc
    self.verbose_info = None


class TimeoutError(NetError):
  """Timeout while reading HTTP response."""


class ConnectionError(NetError):
  """Failed to connect to the server."""


class HttpError(NetError):
  """Server returned HTTP error code."""

  def __init__(self, code, content_type, inner_exc):
    super(HttpError, self).__init__(inner_exc)
    self.code = code
    self.content_type = content_type


def set_engine_class(engine_cls):
  """Globally changes a class to use to execute HTTP requests.

  Default engine is RequestsLibEngine that uses 'requests' library. Changing the
  engine on the fly is not supported. It must be set before the first request.

  Custom engine class should support same public interface as RequestsLibEngine.
  """
  global _request_engine_cls
  assert _request_engine_cls is None
  _request_engine_cls = engine_cls


def get_engine_class():
  """Returns a class to use to execute HTTP requests."""
  return _request_engine_cls or RequestsLibEngine


def url_open(url, **kwargs):  # pylint: disable=W0621
  """Attempts to open the given url multiple times.

  |data| can be either:
    - None for a GET request
    - str for pre-encoded data
    - list for data to be encoded
    - dict for data to be encoded

  See HttpService.request for a full list of arguments.

  Returns HttpResponse object, where the response may be read from, or None
  if it was unable to connect.
  """
  urlhost, urlpath = split_server_request_url(url)
  service = get_http_service(urlhost)
  return service.request(urlpath, **kwargs)


def url_read(url, **kwargs):
  """Attempts to open the given url multiple times and read all data from it.

  Accepts same arguments as url_open function.

  Returns all data read or None if it was unable to connect or read the data.
  """
  kwargs['stream'] = False
  response = url_open(url, **kwargs)
  if not response:
    return None
  try:
    return response.read()
  except TimeoutError:
    return None


def url_read_json(url, **kwargs):
  """Attempts to open the given url multiple times and read all data from it.

  Accepts same arguments as url_open function.

  Returns all data read or None if it was unable to connect or read the data.
  """
  urlhost, urlpath = split_server_request_url(url)
  service = get_http_service(urlhost)
  try:
    return service.json_request(urlpath, **kwargs)
  except TimeoutError:
    return None


def url_retrieve(filepath, url, **kwargs):
  """Downloads an URL to a file. Returns True on success."""
  response = url_open(url, **kwargs)
  if not response:
    return False
  try:
    with open(filepath, 'wb') as f:
      while True:
        buf = response.read(65536)
        if not buf:
          return True
        f.write(buf)
  except (IOError, OSError, TimeoutError):
    try:
      os.remove(filepath)
    except IOError:
      pass
    return False


def split_server_request_url(url):
  """Splits the url into scheme+netloc and path+params+query+fragment."""
  url_parts = list(urlparse.urlparse(url))
  urlhost = '%s://%s' % (url_parts[0], url_parts[1])
  urlpath = urlparse.urlunparse(['', ''] + url_parts[2:])
  return urlhost, urlpath


def fix_url(url):
  """Fixes an url to https."""
  parts = urlparse.urlparse(url, 'https')
  if parts.query:
    raise ValueError('doesn\'t support query parameter.')
  if parts.fragment:
    raise ValueError('doesn\'t support fragment in the url.')
  # urlparse('foo.com') will result in netloc='', path='foo.com', which is not
  # what is desired here.
  new = list(parts)
  if not new[1] and new[2]:
    new[1] = new[2].rstrip('/')
    new[2] = ''
  new[2] = new[2].rstrip('/')
  return urlparse.urlunparse(new)


def get_http_service(urlhost, allow_cached=True):
  """Returns existing or creates new instance of HttpService that can send
  requests to given base urlhost.
  """
  def new_service():
    # Create separate authenticator only if engine is not providing
    # authentication already. Also we use signed URLs for Google Storage, no
    # need for special authentication.
    authenticator = None
    engine_cls = get_engine_class()
    is_gs = GS_STORAGE_HOST_URL_RE.match(urlhost)
    conf = get_oauth_config()
    if not engine_cls.provides_auth and not is_gs and not conf.disabled:
      authenticator = OAuthAuthenticator(urlhost, conf)
    return HttpService(
        urlhost,
        engine=engine_cls(),
        authenticator=authenticator)

  # Ensure consistency in url naming.
  urlhost = str(urlhost).lower().rstrip('/')

  if not allow_cached:
    return new_service()
  with _http_services_lock:
    service = _http_services.get(urlhost)
    if not service:
      service = new_service()
      _http_services[urlhost] = service
    return service


def set_oauth_config(config):
  """Defines what OAuth configuration to use for authentication.

  If request engine (see get_engine_class) provides authentication already (as
  indicated by its 'provides_auth=True' class property) this setting is ignored.

  Arguments:
    config: oauth.OAuthConfig instance.
  """
  global _auth_config
  _auth_config = config


def get_oauth_config():
  """Returns global OAuthConfig as set by 'set_oauth_config' or default one."""
  return _auth_config or oauth.make_oauth_config()


def get_case_insensitive_dict(original):
  """Given a dict with string keys returns new CaseInsensitiveDict.

  Raises ValueError if there are duplicate keys.
  """
  normalized = structures.CaseInsensitiveDict(original or {})
  if len(normalized) != len(original):
    raise ValueError('Duplicate keys in: %s' % repr(original))
  return normalized


class HttpService(object):
  """Base class for a class that provides an API to HTTP based service:
    - Provides 'request' method.
    - Supports automatic request retries.
    - Thread safe.
  """

  def __init__(self, urlhost, engine, authenticator=None):
    self.urlhost = urlhost
    self.engine = engine
    self.authenticator = authenticator

  @staticmethod
  def is_transient_http_error(code, retry_404, retry_50x, suburl, content_type):
    """Returns True if given HTTP response code is a transient error."""
    # Google Storage can return this and it should be retried.
    if code == 408:
      return True
    if code == 404:
      # Retry 404 if allowed by the caller.
      if retry_404:
        return retry_404
      # Transparently retry 404 IIF it is a CloudEndpoints API call *and* the
      # result is not JSON. This assumes that we only use JSON encoding.
      return (
          suburl.startswith('/_ah/api/') and
          not content_type.startswith('application/json'))
    # All other 4** errors are fatal.
    if code < 500:
      return False
    # Retry >= 500 error only if allowed by the caller.
    return retry_50x

  @staticmethod
  def encode_request_body(body, content_type):
    """Returns request body encoded according to its content type."""
    # No body or it is already encoded.
    if body is None or isinstance(body, str):
      return body
    # Any body should have content type set.
    assert content_type, 'Request has body, but no content type'
    encoder = CONTENT_ENCODERS.get(content_type)
    assert encoder, ('Unknown content type %s' % content_type)
    return encoder(body)

  def login(self, allow_user_interaction):
    """Runs authentication flow to refresh short lived access token.

    Authentication flow may need to interact with the user (read username from
    stdin, open local browser for OAuth2, etc.). If interaction is required and
    |allow_user_interaction| is False, the login will silently be considered
    failed (i.e. this function returns False).

    'request' method always uses non-interactive login, so long-lived
    authentication tokens (OAuth2 refresh token, etc) have to be set up
    manually by developer (by calling 'auth.py login' perhaps) prior running
    any swarming or isolate scripts.
    """
    # Use global lock to ensure two authentication flows never run in parallel.
    with _auth_lock:
      if self.authenticator:
        return self.authenticator.login(allow_user_interaction)
      return False

  def logout(self):
    """Purges access credentials from local cache."""
    if self.authenticator:
      self.authenticator.logout()

  def request(
      self,
      urlpath,
      data=None,
      content_type=None,
      max_attempts=URL_OPEN_MAX_ATTEMPTS,
      retry_404=False,
      retry_50x=True,
      timeout=URL_OPEN_TIMEOUT,
      read_timeout=URL_READ_TIMEOUT,
      stream=True,
      method=None,
      headers=None,
      follow_redirects=True):
    """Attempts to open the given url multiple times.

    |urlpath| is relative to the server root, i.e. '/some/request?param=1'.

    |data| can be either:
      - None for a GET request
      - str for pre-encoded data
      - list for data to be form-encoded
      - dict for data to be form-encoded

    - Optionally retries HTTP 404 and 50x.
    - Retries up to |max_attempts| times. If None or 0, there's no limit in the
      number of retries.
    - Retries up to |timeout| duration in seconds. If None or 0, there's no
      limit in the time taken to do retries.
    - If both |max_attempts| and |timeout| are None or 0, this functions retries
      indefinitely.

    If |method| is given it can be 'DELETE', 'GET', 'POST' or 'PUT' and it will
    be used when performing the request. By default it's GET if |data| is None
    and POST if |data| is not None.

    If |headers| is given, it should be a dict with HTTP headers to append
    to request. Caller is responsible for providing headers that make sense.

    If |follow_redirects| is True, will transparently follow HTTP redirects,
    otherwise redirect response will be returned as is. It can be recognized
    by the presence of 'Location' response header.

    If |read_timeout| is not None will configure underlying socket to
    raise TimeoutError exception whenever there's no response from the server
    for more than |read_timeout| seconds. It can happen during any read
    operation so once you pass non-None |read_timeout| be prepared to handle
    these exceptions in subsequent reads from the stream.

    Returns a file-like object, where the response may be read from, or None
    if it was unable to connect. If |stream| is False will read whole response
    into memory buffer before returning file-like object that reads from this
    memory buffer.
    """
    assert urlpath and urlpath[0] == '/', urlpath

    if data is not None:
      assert method in (None, 'DELETE', 'POST', 'PUT')
      method = method or 'POST'
      content_type = content_type or DEFAULT_CONTENT_TYPE
      body = self.encode_request_body(data, content_type)
    else:
      assert method in (None, 'DELETE', 'GET')
      method = method or 'GET'
      body = None
      assert not content_type, 'Can\'t use content_type on %s' % method

    # Prepare request info.
    parsed = urlparse.urlparse('/' + urlpath.lstrip('/'))
    resource_url = urlparse.urljoin(self.urlhost, parsed.path)
    query_params = urlparse.parse_qsl(parsed.query)

    # Prepare headers.
    headers = get_case_insensitive_dict(headers or {})
    if body is not None:
      headers['Content-Length'] = len(body)
      if content_type:
        headers['Content-Type'] = content_type

    last_error = None
    auth_attempted = False

    for attempt in retry_loop(max_attempts, timeout):
      # Log non-first attempt.
      if attempt.attempt:
        logging.warning(
            'Retrying request %s, attempt %d/%d...',
            resource_url, attempt.attempt, max_attempts)

      try:
        # Prepare and send a new request.
        request = HttpRequest(
            method, resource_url, query_params, body,
            headers, read_timeout, stream, follow_redirects)
        if self.authenticator:
          self.authenticator.authorize(request)
        response = self.engine.perform_request(request)
        response._timeout_exc_classes = self.engine.timeout_exception_classes()
        logging.debug('Request %s succeeded', request.get_full_url())
        return response

      except (ConnectionError, TimeoutError) as e:
        last_error = e
        logging.warning(
            'Unable to open url %s on attempt %d.\n%s',
            request.get_full_url(), attempt.attempt, self._format_error(e))
        continue

      except HttpError as e:
        last_error = e

        # Access denied -> authenticate.
        if e.code in (401, 403):
          logging.warning(
              'Authentication is required for %s on attempt %d.\n%s',
              request.get_full_url(), attempt.attempt, self._format_error(e))
          # Try forcefully refresh the token. If it doesn't help, then server
          # does not support authentication or user doesn't have required
          # access.
          if not auth_attempted:
            auth_attempted = True
            if self.login(allow_user_interaction=False):
              # Success! Run request again immediately.
              attempt.skip_sleep = True
              continue
          # Authentication attempt was unsuccessful.
          logging.error(
              'Unable to authenticate to %s (%s).',
              self.urlhost, self._format_error(e))
          if self.authenticator:
            logging.error(
                'Use auth.py to login: python auth.py login --service=%s',
                self.urlhost)
          return None

        # Hit a error that can not be retried -> stop retry loop.
        if not self.is_transient_http_error(
            e.code, retry_404, retry_50x, parsed.path, e.content_type):
          # This HttpError means we reached the server and there was a problem
          # with the request, so don't retry.
          logging.warning(
              'Able to connect to %s but an exception was thrown.\n%s',
              request.get_full_url(), self._format_error(e, verbose=True))
          return None

        # Retry all other errors.
        logging.warning(
            'Server responded with error on %s on attempt %d.\n%s',
            request.get_full_url(), attempt.attempt, self._format_error(e))
        continue

    logging.error(
        'Unable to open given url, %s, after %d attempts.\n%s',
        request.get_full_url(), max_attempts,
        self._format_error(last_error, verbose=True))
    return None

  def json_request(self, urlpath, data=None, **kwargs):
    """Sends JSON request to the server and parses JSON response it get back.

    Arguments:
      urlpath: relative request path (e.g. '/auth/v1/...').
      data: object to serialize to JSON and sent in the request.

    See self.request() for more details.

    Returns:
      Deserialized JSON response on success, None on error or timeout.
    """
    content_type = JSON_CONTENT_TYPE if data is not None else None
    response = self.request(
        urlpath, content_type=content_type, data=data, stream=False, **kwargs)
    if not response:
      return None
    try:
      text = response.read()
      if not text:
        return None
    except TimeoutError:
      return None
    try:
      return json.loads(text)
    except ValueError as e:
      logging.error('Not a JSON response when calling %s: %s; full text: %s',
                    urlpath, e, text)
      return None

  def _format_error(self, exc, verbose=False):
    """Returns readable description of a NetError."""
    if not isinstance(exc, NetError):
      return str(exc)
    if not verbose:
      return str(exc.inner_exc or exc)
    # Avoid making multiple calls to parse_request_exception since they may
    # have side effects on the exception, e.g. urllib2 based exceptions are in
    # fact file-like objects that can not be read twice.
    if exc.verbose_info is None:
      out = [str(exc.inner_exc or exc)]
      headers, body = self.engine.parse_request_exception(exc.inner_exc)
      if headers or body:
        out.append('----------')
        if headers:
          for header, value in headers:
            if not header.startswith('x-'):
              out.append('%s: %s' % (header.capitalize(), value))
          out.append('')
        out.append(body or '<empty body>')
        out.append('----------')
      exc.verbose_info = '\n'.join(out)
    return exc.verbose_info


class HttpRequest(object):
  """Request to HttpService."""

  def __init__(
      self, method, url, params, body,
      headers, timeout, stream, follow_redirects):
    """Arguments:
      |method| - HTTP method to use
      |url| - relative URL to the resource, without query parameters
      |params| - list of (key, value) pairs to put into GET parameters
      |body| - encoded body of the request (None or str)
      |headers| - dict with request headers
      |timeout| - socket read timeout (None to disable)
      |stream| - True to stream response from socket
      |follow_redirects| - True to follow HTTP redirects.
    """
    self.method = method
    self.url = url
    self.params = params[:]
    self.body = body
    self.headers = headers.copy()
    self.timeout = timeout
    self.stream = stream
    self.follow_redirects = follow_redirects
    self._cookies = None

  @property
  def cookies(self):
    """CookieJar object that will be used for cookies in this request."""
    if self._cookies is None:
      self._cookies = cookielib.CookieJar()
    return self._cookies

  def get_full_url(self):
    """Resource URL with url-encoded GET parameters."""
    if not self.params:
      return self.url
    else:
      return '%s?%s' % (self.url, urllib.urlencode(self.params))

  def make_fake_response(self, content='', headers=None):
    """Makes new fake HttpResponse to this request, useful in tests."""
    return HttpResponse.get_fake_response(content, self.get_full_url(), headers)


class HttpResponse(object):
  """Response from HttpService."""

  def __init__(self, stream, url, headers):
    self._stream = stream
    self._url = url
    self._headers = get_case_insensitive_dict(headers)
    self._read = 0
    self._timeout_exc_classes = ()

  @property
  def content_length(self):
    """Total length to the response or None if not known in advance."""
    length = self.get_header('Content-Length')
    return int(length) if length is not None else None

  def get_header(self, header):
    """Returns response header (as str) or None if no such header."""
    return self._headers.get(header)

  def read(self, size=None):
    """Reads up to |size| bytes from the stream and returns them.

    If |size| is None reads all available bytes.

    Raises TimeoutError on read timeout.
    """
    assert isinstance(self._timeout_exc_classes, tuple)
    assert all(issubclass(e, Exception) for e in self._timeout_exc_classes)
    try:
      # cStringIO has a bug: stream.read(None) is not the same as stream.read().
      data = self._stream.read() if size is None else self._stream.read(size)
      self._read += len(data)
      return data
    except self._timeout_exc_classes as e:
      logging.error('Timeout while reading from %s, read %d of %s: %s',
          self._url, self._read, self.content_length, e)
      raise TimeoutError(e)

  @classmethod
  def get_fake_response(cls, content, url, headers=None):
    """Returns HttpResponse with predefined content, useful in tests."""
    headers = dict(headers or {})
    headers['Content-Length'] = len(content)
    return cls(StringIO.StringIO(content), url, headers)


class Authenticator(object):
  """Base class for objects that know how to authenticate into http services."""

  def authorize(self, request):
    """Add authentication information to the request."""

  def login(self, allow_user_interaction):
    """Run interactive authentication flow refreshing the token."""
    raise NotImplementedError()

  def logout(self):
    """Purges access credentials from local cache."""


class RequestsLibEngine(object):
  """Class that knows how to execute HttpRequests via requests library."""

  # This engine doesn't know how to authenticate requests on transport level.
  provides_auth = False

  @classmethod
  def parse_request_exception(cls, exc):
    """Extracts HTTP headers and body from inner exceptions put in HttpError."""
    if isinstance(exc, requests.HTTPError):
      return exc.response.headers.items(), exc.response.content
    return None, None

  @classmethod
  def timeout_exception_classes(cls):
    """A tuple of exception classes that represent timeout.

    Will be caught while reading a streaming response in HttpResponse.read and
    transformed to TimeoutError.
    """
    return (
        socket.timeout, ssl.SSLError, requests.Timeout,
        requests.packages.urllib3.exceptions.ProtocolError,
        requests.packages.urllib3.exceptions.TimeoutError)

  def __init__(self):
    super(RequestsLibEngine, self).__init__()
    self.session = requests.Session()
    # Configure session.
    self.session.trust_env = False
    self.session.verify = tools.get_cacerts_bundle()
    # Configure connection pools.
    for protocol in ('https://', 'http://'):
      self.session.mount(protocol, adapters.HTTPAdapter(
          pool_connections=64,
          pool_maxsize=64,
          max_retries=0,
          pool_block=False))

  def perform_request(self, request):
    """Sends a HttpRequest to the server and reads back the response.

    Returns HttpResponse.

    Raises:
      ConnectionError - failed to establish connection to the server.
      TimeoutError - timeout while connecting or reading response.
      HttpError - server responded with >= 400 error code.
    """
    try:
      # response is a requests.models.Response.
      response = self.session.request(
          method=request.method,
          url=request.url,
          params=request.params,
          data=request.body,
          headers=request.headers,
          cookies=request.cookies,
          timeout=request.timeout,
          stream=request.stream,
          allow_redirects=request.follow_redirects)
      response.raise_for_status()
      if request.stream:
        stream = response.raw
      else:
        stream = StringIO.StringIO(response.content)
      return HttpResponse(stream, request.get_full_url(), response.headers)
    except requests.Timeout as e:
      raise TimeoutError(e)
    except requests.HTTPError as e:
      raise HttpError(
          e.response.status_code, e.response.headers.get('Content-Type'), e)
    except (requests.ConnectionError, socket.timeout, ssl.SSLError) as e:
      raise ConnectionError(e)


class OAuthAuthenticator(Authenticator):
  """Uses OAuth Authorization header to authenticate requests."""

  def __init__(self, urlhost, config):
    super(OAuthAuthenticator, self).__init__()
    assert isinstance(config, oauth.OAuthConfig)
    self.urlhost = urlhost
    self.config = config
    self._lock = threading.Lock()
    self._access_token = None

  def authorize(self, request):
    with self._lock:
      # Load from cache on a first access.
      if not self._access_token:
        self._access_token = oauth.load_access_token(self.urlhost, self.config)
      # Refresh if expired.
      need_refresh = True
      if self._access_token:
        if self._access_token.expires_at is not None:
          # Allow 5 min of clock skew.
          now = datetime.datetime.utcnow() + datetime.timedelta(seconds=300)
          need_refresh = now >= self._access_token.expires_at
        else:
          # Token without expiration time never expired.
          need_refresh = False
      if need_refresh:
        self._access_token = oauth.create_access_token(
            self.urlhost, self.config, False)
      if self._access_token:
        request.headers['Authorization'] = (
            'Bearer %s' % self._access_token.token)

  def login(self, allow_user_interaction):
    with self._lock:
      # Forcefully refresh the token.
      self._access_token = oauth.create_access_token(
          self.urlhost, self.config, allow_user_interaction)
      return self._access_token is not None

  def logout(self):
    with self._lock:
      self._access_token = None
      oauth.purge_access_token(self.urlhost, self.config)


class RetryAttempt(object):
  """Contains information about current retry attempt.

  Yielded from retry_loop.
  """

  def __init__(self, attempt, remaining):
    """Information about current attempt in retry loop:
      |attempt| - zero based index of attempt.
      |remaining| - how much time is left before retry loop finishes retries.
    """
    self.attempt = attempt
    self.remaining = remaining
    self.skip_sleep = False


def calculate_sleep_before_retry(attempt, max_duration):
  """How long to sleep before retrying an attempt in retry_loop."""
  # Maximum sleeping time. We're hammering a cloud-distributed service, it'll
  # survive.
  MAX_SLEEP = 10.
  # random.random() returns [0.0, 1.0). Starts with relatively short waiting
  # time by starting with 1.5/2+1.5^-1 median offset.
  duration = (random.random() * 1.5) + math.pow(1.5, (attempt - 1))
  assert duration > 0.1
  duration = min(MAX_SLEEP, duration)
  if max_duration:
    duration = min(max_duration, duration)
  return duration


def sleep_before_retry(attempt, max_duration):
  """Sleeps for some amount of time when retrying the attempt in retry_loop.

  To be mocked in tests.
  """
  time.sleep(calculate_sleep_before_retry(attempt, max_duration))


def current_time():
  """Used by retry loop to get current time.

  To be mocked in tests.
  """
  return time.time()


def retry_loop(max_attempts=None, timeout=None):
  """Yields whenever new attempt to perform some action is needed.

  Yields instances of RetryAttempt class that contains information about current
  attempt. Setting |skip_sleep| attribute of RetryAttempt to True will cause
  retry loop to run next attempt immediately.
  """
  start = current_time()
  for attempt in itertools.count():
    # Too many attempts?
    if max_attempts and attempt == max_attempts:
      break
    # Retried for too long?
    remaining = (timeout - (current_time() - start)) if timeout else None
    if remaining is not None and remaining < 0:
      break
    # Kick next iteration.
    attemp_obj = RetryAttempt(attempt, remaining)
    yield attemp_obj
    if attemp_obj.skip_sleep:
      continue
    # Only sleep if we are going to try again.
    if max_attempts and attempt != max_attempts - 1:
      remaining = (timeout - (current_time() - start)) if timeout else None
      if remaining is not None and remaining < 0:
        break
      sleep_before_retry(attempt, remaining)
