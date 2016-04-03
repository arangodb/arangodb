#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Profiler to compare various compression levels with regards to speed
and final size when compressing the full set of files from a given
isolated file.
"""

import bz2
import optparse
import os
import subprocess
import sys
import tempfile
import time
import zlib

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from third_party.depot_tools import fix_encoding
from utils import file_path
from utils import tools


def zip_file(compressor_constructor, compression_level, filename):
  compressed_size = 0
  compressor = compressor_constructor(compression_level)
  with open(filename, 'rb') as f:
    while True:
      chunk = f.read(16 * 1024)
      if not chunk:
        break
      compressed_size += len(compressor.compress(f.read()))
    compressed_size += len(compressor.flush())

  return compressed_size


def zip_directory(compressor_constructor, compression_level, root_dir):
  compressed_size = 0
  for root, _, files in os.walk(root_dir):
    compressed_size += sum(zip_file(compressor_constructor, compression_level,
                                    os.path.join(root, name))
                           for name in files)
  return compressed_size


def profile_compress(zip_module_name, compressor_constructor,
                     compression_range, compress_func, compress_target):
  for i in compression_range:
    start_time = time.time()
    compressed_size = compress_func(compressor_constructor, i, compress_target)
    end_time = time.time()

    print('%4s at compression level %s, total size %11d, time taken %6.3f' %
          (zip_module_name, i, compressed_size, end_time - start_time))


def tree_files(root_dir):
  file_set = {}
  for root, _, files in os.walk(root_dir):
    for name in files:
      filename = os.path.join(root, name)
      file_set[filename] = os.stat(filename).st_size

  return file_set


def main():
  tools.disable_buffering()
  parser = optparse.OptionParser()
  parser.add_option('-s', '--isolated', help='.isolated file to profile with.')
  parser.add_option('--largest_files', type='int',
                    help='If this is set, instead of compressing all the '
                    'files, only the large n files will be compressed')
  options, args = parser.parse_args()

  if args:
    parser.error('Unknown args passed in; %s' % args)
  if not options.isolated:
    parser.error('The .isolated file must be given.')

  temp_dir = None
  try:
    temp_dir = tempfile.mkdtemp(prefix=u'zip_profiler')

    # Create a directory of the required files
    subprocess.check_call([os.path.join(ROOT_DIR, 'isolate.py'),
                           'remap',
                           '-s', options.isolated,
                           '--outdir', temp_dir])

    file_set = tree_files(temp_dir)

    if options.largest_files:
      sorted_by_size = sorted(file_set.iteritems(),  key=lambda x: x[1],
                              reverse=True)
      files_to_compress = sorted_by_size[:options.largest_files]

      for filename, size in files_to_compress:
        print('Compressing %s, uncompressed size %d' % (filename, size))

        profile_compress('zlib', zlib.compressobj, range(10), zip_file,
                         filename)
        profile_compress('bz2', bz2.BZ2Compressor, range(1, 10), zip_file,
                         filename)
    else:
      print('Number of files: %s' % len(file_set))
      print('Total size: %s' % sum(file_set.itervalues()))

      # Profile!
      profile_compress('zlib', zlib.compressobj, range(10), zip_directory,
                       temp_dir)
      profile_compress('bz2', bz2.BZ2Compressor, range(1, 10), zip_directory,
                       temp_dir)
  finally:
    file_path.rmtree(temp_dir)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  sys.exit(main())
