# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Functions in this file relies on depot_tools been checked-out as a sibling
# of infra.git.

import logging
import os
import re
import subprocess


BASE_DIR = os.path.dirname(
  os.path.dirname(
    os.path.dirname(
      os.path.dirname(os.path.realpath(__file__)))))


def parse_revinfo(revinfo):
  """Parse the output of "gclient revinfo -a"

  Args:
    revinfo (str): string containing gclient stdout.

  Returns:
    revinfo_d (dict): <directory>: (URL, revision)
  """
  revision_expr = re.compile('(.*)@([^@]*)')

  revinfo_d = {}
  for line in revinfo.splitlines():
    if ':' not in line:
      continue

    # TODO: this fails when the file name contains a colon.
    path, line = line.split(':', 1)
    if '@' in line:
      url, revision = revision_expr.match(line).groups()
      revision = revision.strip()
    else:
      # Split at the last @
      url, revision = line.strip(), None

    path = path.strip()
    url = url.strip()
    revinfo_d[path] = {'source_url': url, 'revision': revision}
  return revinfo_d


def get_revinfo(cwd=None):  # pragma: no cover
  """Call gclient to get the list of all revisions actually checked out on disk.

  gclient is expected to be under depot_tools/ sibling to infra/.
  If gclient can't be found or fail to run returns {}.

  Args:
    cwd (str): working directory where to run gclient. If None, use the
      current working directory.
  Returns:
    revinfo (dict): keys are local paths, values are dicts with keys:
      'source_url' or 'revision'. The latter can be a git SHA1 or an svn
      revision.
  """

  cmd = [os.path.join(BASE_DIR, 'depot_tools', 'gclient'), 'revinfo', '-a']
  logging.debug('Running: %s', ' '.join(cmd))
  revinfo = ''
  try:
    revinfo = subprocess.check_output(cmd, cwd=cwd)
  except (subprocess.CalledProcessError, OSError):
    logging.exception('Command failed to run: %s', ' '.join(cmd))
  return parse_revinfo(revinfo)
