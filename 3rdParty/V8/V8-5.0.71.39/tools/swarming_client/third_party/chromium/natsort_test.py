#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for nasort.py."""

import doctest
import os
import sys

import natsort


if __name__ == '__main__':
  doctest.testmod(natsort)
