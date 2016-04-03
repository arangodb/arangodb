# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Top-level presubmit script for swarm_client.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

def CommonChecks(input_api, output_api):
  import sys
  def join(*args):
    return input_api.os_path.join(input_api.PresubmitLocalPath(), *args)

  output = []
  sys_path_backup = sys.path
  try:
    sys.path = [
      input_api.PresubmitLocalPath(),
      join('tests'),
      join('third_party'),
    ] + sys.path
    output.extend(input_api.canned_checks.RunPylint(input_api, output_api))
  finally:
    sys.path = sys_path_backup

  # These tests are touching the live infrastructure. It's a pain if your IP
  # is not whitelisted so do not run them for now. They should use a local fake
  # web service instead.
  blacklist = [
    r'.*isolateserver_smoke_test\.py$',
    r'.*swarming_smoke_test\.py$',
  ]
  if not input_api.is_committing:
    # Remove all slow tests, e.g. the ones that take >1s to complete.
    blacklist.extend([
      r'.*isolate_smoke_test\.py$',
      r'.*trace_inputs_smoke_test\.py$',
      r'.*url_open_timeout_test\.py$',
    ])

  output.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api, output_api,
          input_api.os_path.join(input_api.PresubmitLocalPath(), 'tests'),
          whitelist=[r'.+_test\.py$'],
          blacklist=blacklist))
  return output


def header(input_api):
  """Returns the expected license header regexp for this project."""
  current_year = int(input_api.time.strftime('%Y'))
  allowed_years = (str(s) for s in reversed(xrange(2011, current_year + 1)))
  years_re = '(' + '|'.join(allowed_years) + ')'
  license_header = (
    r'.*? Copyright %(year)s The Swarming Authors\. '
      r'All rights reserved\.\n'
    r'.*? Use of this source code is governed under the Apache License, '
      r'Version 2\.0 that\n'
    r'.*? can be found in the LICENSE file\.(?: \*/)?\n'
  ) % {
    'year': years_re,
  }
  return license_header


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  output = CommonChecks(input_api, output_api)
  output.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api,
      owners_check=False,
      license_header=header(input_api)))
  return output
