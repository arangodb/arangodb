# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Caching utility for the discovery document."""

from __future__ import absolute_import

import logging
import datetime

DISCOVERY_DOC_MAX_AGE = 60 * 60 * 24  # 1 day


def autodetect():
  """Detects an appropriate cache module and returns it.

  Returns:
    googleapiclient.discovery_cache.base.Cache, a cache object which
    is auto detected, or None if no cache object is available.
  """
  try:
    from google.appengine.api import memcache
    from . import appengine_memcache
    return appengine_memcache.cache
  except Exception:
    try:
      from . import file_cache
      return file_cache.cache
    except Exception as e:
      logging.warning(e, exc_info=True)
      return None
