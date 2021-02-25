#!/bin/sh
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

treeroot="$(dirname "$0")/.."
cd "${treeroot}"

echo "Applying brkitr.patch"
patch -p1 < flutter/brkitr.patch || { echo "failed to patch" >&2; exit 1; }
