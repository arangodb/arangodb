# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

# GN only supports shelling to python. Until update.py is used on all
# platforms (currently only Windows), wrap update.sh.
sys.exit(os.system(os.path.join(os.path.dirname(__file__), 'update.sh') +
                   ' --print-revision'))
