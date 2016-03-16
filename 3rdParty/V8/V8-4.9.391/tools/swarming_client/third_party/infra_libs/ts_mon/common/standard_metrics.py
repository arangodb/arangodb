# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Metrics common to all tasks and devices."""

from infra_libs.ts_mon.common import metrics


up = metrics.BooleanMetric(
    'presence/up',
    description="Set to True when the program is running, missing otherwise.")


def init():
  # TODO(dsansome): Add more metrics for git revision, cipd package version,
  # uptime, etc.
  up.set(True)
