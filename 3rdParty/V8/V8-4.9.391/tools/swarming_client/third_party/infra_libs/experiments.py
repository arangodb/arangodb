# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tools for gradually enabling a feature on a deterministic set of machines.

Add a flag to your program to control the percentage of machines that a new
feature should be enabled on::

    def add_argparse_options(self, parser):
      parser.add_argument('--myfeature-percent', type=int, default=0)

    def main(self, opts):
      if experiments.is_active_for_host('myfeature', opts.myfeature_percent):
        # do myfeature
"""

import hashlib
import logging
import socket
import struct


def _is_active(labels, percent):
  h = hashlib.md5()
  for label, value in sorted(labels.iteritems()):
    h.update(label)
    h.update(value)

  # The first 8 bytes of the hash digest as an unsigned integer.
  hash_num = struct.unpack_from('Q', h.digest())[0]

  return (hash_num % 100) < percent


def is_active_for_host(experiment_name, percent):
  ret = _is_active({
      'name': experiment_name,
      'host': socket.getfqdn(),
  }, percent)

  if ret:
    logging.info('Experiment "%s" is active', experiment_name)

  return ret
