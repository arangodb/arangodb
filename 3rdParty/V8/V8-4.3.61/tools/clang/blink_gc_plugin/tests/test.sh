#!/bin/bash
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Hacky, primitive testing: This runs the style plugin for a set of input files
# and compares the output with golden result files.

E_BADARGS=65
E_FAILEDTEST=1

failed_any_test=

# Prints usage information.
usage() {
  echo "Usage: $(basename "${0}")" \
    "<path to clang>" \
    "<path to plugin>"
  echo ""
  echo "  Runs all the libBlinkGCPlugin unit tests"
  echo ""
}

# Runs a single test case.
do_testcase() {
  local flags=""
  if [ -e "${3}" ]; then
    flags="$(cat "${3}")"
  fi
  local output="$("${CLANG_PATH}" -c -Wno-c++11-extensions \
      -Wno-inaccessible-base \
      -Xclang -load -Xclang "${PLUGIN_PATH}" \
      -Xclang -add-plugin -Xclang blink-gc-plugin ${flags} ${1} 2>&1)"
  local json="${input%cpp}graph.json"
  if [ -f "$json" ]; then
    output="$(python ../process-graph.py -c ${json} 2>&1)"
  fi
  local diffout="$(echo "${output}" | diff - "${2}")"
  if [ "${diffout}" = "" ]; then
    echo "PASS: ${1}"
  else
    failed_any_test=yes
    echo "FAIL: ${1}"
    echo "Output of compiler:"
    echo "${output}"
    echo "Expected output:"
    cat "${2}"
    echo
  fi
}

# Validate input to the script.
if [[ -z "${1}" ]]; then
  usage
  exit ${E_BADARGS}
elif [[ -z "${2}" ]]; then
  usage
  exit ${E_BADARGS}
elif [[ ! -x "${1}" ]]; then
  echo "${1} is not an executable"
  usage
  exit ${E_BADARGS}
elif [[ ! -f "${2}" ]]; then
  echo "${2} could not be found"
  usage
  exit ${E_BADARGS}
else
  export CLANG_PATH="${1}"
  export PLUGIN_PATH="${2}"
  echo "Using clang ${CLANG_PATH}..."
  echo "Using plugin ${PLUGIN_PATH}..."

  # The golden files assume that the cwd is this directory. To make the script
  # work no matter what the cwd is, explicitly cd to there.
  cd "$(dirname "${0}")"
fi

for input in *.cpp; do
  do_testcase "${input}" "${input%cpp}txt" "${input%cpp}flags"
done

if [[ "${failed_any_test}" ]]; then
  exit ${E_FAILEDTEST}
fi
