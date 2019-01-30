#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Adds code coverage flags to the invocations of the Clang C/C++ compiler.

This script is used to instrument a subset of the source files, and the list of
files to instrument is specified by an input file that is passed to this script
as a command-line argument.

The path to the coverage instrumentation input file should be relative to the
root build directory, and the file consists of multiple lines where each line
represents a path to a source file, and the specified paths must be relative to
the root build directory. e.g. ../../base/task/post_task.cc for build
directory 'out/Release'.

One caveat with this compiler wrapper is that it may introduce unexpected
behaviors in incremental builds when the file path to the coverage
instrumentation input file changes between consecutive runs, so callers of this
script are strongly advised to always use the same path such as
"${root_build_dir}/coverage_instrumentation_input.txt".

It's worth noting on try job builders, if the contents of the instrumentation
file changes so that a file doesn't need to be instrumented any longer, it will
be recompiled automatically because if try job B runs after try job A, the files
that were instrumented in A will be updated (i.e., reverted to the checked in
version) in B, and so they'll be considered out of date by ninja and recompiled.

Example usage:
  clang_code_coverage_wrapper.py \\
      --files-to-instrument=coverage_instrumentation_input.txt
"""

import argparse
import os
import subprocess
import sys

# Flags used to enable coverage instrumentation.
_COVERAGE_FLAGS = ['-fprofile-instr-generate', '-fcoverage-mapping']


def main():
  # TODO(crbug.com/898695): Make this wrapper work on Windows platform.
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__
  arg_parser.add_argument(
      '--files-to-instrument',
      type=str,
      required=True,
      help='Path to a file that contains a list of file names to instrument.')
  arg_parser.add_argument('args', nargs=argparse.REMAINDER)
  parsed_args = arg_parser.parse_args()

  if not os.path.isfile(parsed_args.files_to_instrument):
    raise Exception('Path to the coverage instrumentation file: "%s" doesn\'t '
                    'exist.' % parsed_args.files_to_instrument)

  compile_command = parsed_args.args
  try:
    # The command is assumed to use Clang as the compiler, and the path to the
    # source file is behind the -c argument, and the path to the source path is
    # relative to the root build directory. For example:
    # clang++ -fvisibility=hidden -c ../../base/files/file_path.cc -o \
    #   obj/base/base/file_path.o
    index_dash_c = compile_command.index('-c')
  except ValueError:
    print '-c argument is not found in the compile command.'
    raise

  if index_dash_c + 1 >= len(compile_command):
    raise Exception('Source file to be compiled is missing from the command.')

  compile_source_file = compile_command[index_dash_c + 1]
  with open(parsed_args.files_to_instrument) as f:
    if compile_source_file + '\n' in f.read():
      compile_command.extend(_COVERAGE_FLAGS)

  return subprocess.call(compile_command)


if __name__ == '__main__':
  sys.exit(main())
