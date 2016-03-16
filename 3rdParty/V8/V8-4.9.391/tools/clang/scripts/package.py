#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script will check out llvm and clang, and then package the results up
to a tgz file."""

import argparse
import fnmatch
import itertools
import os
import shutil
import subprocess
import sys
import tarfile

# Path constants.
THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
THIRD_PARTY_DIR = os.path.join(THIS_DIR, '..', '..', '..', 'third_party')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
LLVM_BUILD_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-build')
LLVM_RELEASE_DIR = os.path.join(LLVM_BUILD_DIR, 'Release+Asserts')
STAMP_FILE = os.path.join(LLVM_BUILD_DIR, 'cr_build_revision')


def Tee(output, logfile):
  logfile.write(output)
  print output,


def TeeCmd(cmd, logfile, fail_hard=True):
  """Runs cmd and writes the output to both stdout and logfile."""
  # Reading from PIPE can deadlock if one buffer is full but we wait on a
  # different one.  To work around this, pipe the subprocess's stderr to
  # its stdout buffer and don't give it a stdin.
  # shell=True is required in cmd.exe since depot_tools has an svn.bat, and
  # bat files only work with shell=True set.
  proc = subprocess.Popen(cmd, bufsize=1, shell=sys.platform == 'win32',
                          stdin=open(os.devnull), stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
  for line in iter(proc.stdout.readline,''):
    Tee(line, logfile)
    if proc.poll() is not None:
      break
  exit_code = proc.wait()
  if exit_code != 0 and fail_hard:
    print 'Failed:', cmd
    sys.exit(1)


def PrintTarProgress(tarinfo):
  print 'Adding', tarinfo.name
  return tarinfo


def GetExpectedStamp():
  rev_cmd = [sys.executable, os.path.join(THIS_DIR, 'update.py'),
             '--print-revision']
  return subprocess.check_output(rev_cmd).rstrip()


def GetGsutilPath():
  if not 'find_depot_tools' in sys.modules:
    sys.path.insert(0, os.path.join(CHROMIUM_DIR, 'build'))
    global find_depot_tools
    import find_depot_tools
  depot_path = find_depot_tools.add_depot_tools_to_path()
  if depot_path is None:
    print ('depot_tools are not found in PATH. '
           'Follow the instructions in this document '
           'http://dev.chromium.org/developers/how-tos/install-depot-tools'
           ' to install depot_tools and then try again.')
    sys.exit(1)
  gsutil_path = os.path.join(depot_path, 'gsutil.py')
  return gsutil_path


def RunGsutil(args):
  return subprocess.call([sys.executable, GetGsutilPath()] + args)


def GsutilArchiveExists(archive_name, platform):
  gsutil_args = ['-q', 'stat',
                 'gs://chromium-browser-clang/%s/%s.tgz' %
                 (platform, archive_name)]
  return RunGsutil(gsutil_args) == 0


def MaybeUpload(args, archive_name, platform):
  # We don't want to rewrite the file, if it already exists on the server,
  # so -n option to gsutil is used. It will warn, if the upload was aborted.
  gsutil_args = ['cp', '-n', '-a', 'public-read',
                  '%s.tgz' % archive_name,
                  'gs://chromium-browser-clang/%s/%s.tgz' %
                 (platform, archive_name)]
  if args.upload:
    print 'Uploading %s to Google Cloud Storage...' % archive_name
    exit_code = RunGsutil(gsutil_args)
    if exit_code != 0:
      print "gsutil failed, exit_code: %s" % exit_code
      os.exit(exit_code)
  else:
    print 'To upload, run:'
    print ('gsutil %s' % ' '.join(gsutil_args))


def main():
  parser = argparse.ArgumentParser(description='build and package clang')
  parser.add_argument('--upload', action='store_true',
                      help='Upload the target archive to Google Cloud Storage.')
  args = parser.parse_args()

  # Check that the script is not going to upload a toolchain built from HEAD.
  use_head_revision = 'LLVM_FORCE_HEAD_REVISION' in os.environ
  if args.upload and use_head_revision:
    print ("--upload and LLVM_FORCE_HEAD_REVISION could not be used "
           "at the same time.")
    return 1

  expected_stamp = GetExpectedStamp()
  pdir = 'clang-' + expected_stamp
  golddir = 'llvmgold-' + expected_stamp
  print pdir

  if sys.platform == 'darwin':
    platform = 'Mac'
  elif sys.platform == 'win32':
    platform = 'Win'
  else:
    platform = 'Linux_x64'

  # Check if Google Cloud Storage already has the artifacts we want to build.
  if (args.upload and GsutilArchiveExists(pdir, platform) and
      not sys.platform.startswith('linux') or
      GsutilArchiveExists(golddir, platform)):
    print ('Desired toolchain revision %s is already available '
           'in Google Cloud Storage:') % expected_stamp
    print 'gs://chromium-browser-clang/%s/%s.tgz' % (platform, pdir)
    if sys.platform.startswith('linux'):
      print 'gs://chromium-browser-clang/%s/%s.tgz' % (platform, golddir)
    return 0

  with open('buildlog.txt', 'w') as log:
    Tee('Diff in llvm:\n', log)
    TeeCmd(['svn', 'stat', LLVM_DIR], log, fail_hard=False)
    TeeCmd(['svn', 'diff', LLVM_DIR], log, fail_hard=False)
    Tee('Diff in llvm/tools/clang:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'tools', 'clang')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'tools', 'clang')],
           log, fail_hard=False)
    # TODO(thakis): compiler-rt is in projects/compiler-rt on Windows but
    # llvm/compiler-rt elsewhere. So this diff call is currently only right on
    # Windows.
    Tee('Diff in llvm/compiler-rt:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'projects', 'compiler-rt')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'projects', 'compiler-rt')],
           log, fail_hard=False)
    Tee('Diff in llvm/projects/libcxx:\n', log)
    TeeCmd(['svn', 'stat', os.path.join(LLVM_DIR, 'projects', 'libcxx')],
           log, fail_hard=False)
    TeeCmd(['svn', 'diff', os.path.join(LLVM_DIR, 'projects', 'libcxx')],
           log, fail_hard=False)

    Tee('Starting build\n', log)

    # Do a clobber build.
    shutil.rmtree(LLVM_BOOTSTRAP_DIR, ignore_errors=True)
    shutil.rmtree(LLVM_BOOTSTRAP_INSTALL_DIR, ignore_errors=True)
    shutil.rmtree(LLVM_BUILD_DIR, ignore_errors=True)

    build_cmd = [sys.executable, os.path.join(THIS_DIR, 'update.py'),
                 '--bootstrap', '--force-local-build', '--run-tests']
    TeeCmd(build_cmd, log)

  stamp = open(STAMP_FILE).read().rstrip()
  if stamp != expected_stamp:
    print 'Actual stamp (%s) != expected stamp (%s).' % (stamp, expected_stamp)
    return 1

  shutil.rmtree(pdir, ignore_errors=True)

  # Copy a whitelist of files to the directory we're going to tar up.
  # This supports the same patterns that the fnmatch module understands.
  exe_ext = '.exe' if sys.platform == 'win32' else ''
  want = ['bin/llvm-symbolizer' + exe_ext,
          'lib/clang/*/asan_blacklist.txt',
          'lib/clang/*/cfi_blacklist.txt',
          # Copy built-in headers (lib/clang/3.x.y/include).
          'lib/clang/*/include/*',
          ]
  if sys.platform == 'win32':
    want.append('bin/clang-cl.exe')
    want.append('bin/lld-link.exe')
  else:
    so_ext = 'dylib' if sys.platform == 'darwin' else 'so'
    want.extend(['bin/clang',
                 'lib/libFindBadConstructs.' + so_ext,
                 'lib/libBlinkGCPlugin.' + so_ext,
                 ])
  if sys.platform == 'darwin':
    want.extend([# Copy only the OSX (ASan and profile) and iossim (ASan)
                 # runtime libraries:
                 'lib/clang/*/lib/darwin/*asan_osx*',
                 'lib/clang/*/lib/darwin/*asan_iossim*',
                 'lib/clang/*/lib/darwin/*profile_osx*',
                 ])
  elif sys.platform.startswith('linux'):
    # Copy the libstdc++.so.6 we linked Clang against so it can run.
    want.append('lib/libstdc++.so.6')
    # Copy only
    # lib/clang/*/lib/linux/libclang_rt.{[atm]san,san,ubsan,profile}-*.a ,
    # but not dfsan.
    want.extend(['lib/clang/*/lib/linux/*[atm]san*',
                 'lib/clang/*/lib/linux/*ubsan*',
                 'lib/clang/*/lib/linux/*libclang_rt.san*',
                 'lib/clang/*/lib/linux/*profile*',
                 'lib/clang/*/msan_blacklist.txt',
                 ])
  elif sys.platform == 'win32':
    want.extend(['lib/clang/*/lib/windows/clang_rt.asan*.dll',
                 'lib/clang/*/lib/windows/clang_rt.asan*.lib',
                 'lib/clang/*/include_sanitizer/*',
                 ])

  for root, dirs, files in os.walk(LLVM_RELEASE_DIR):
    # root: third_party/llvm-build/Release+Asserts/lib/..., rel_root: lib/...
    rel_root = root[len(LLVM_RELEASE_DIR)+1:]
    rel_files = [os.path.join(rel_root, f) for f in files]
    wanted_files = list(set(itertools.chain.from_iterable(
        fnmatch.filter(rel_files, p) for p in want)))
    if wanted_files:
      # Guaranteed to not yet exist at this point:
      os.makedirs(os.path.join(pdir, rel_root))
    for f in wanted_files:
      src = os.path.join(LLVM_RELEASE_DIR, f)
      dest = os.path.join(pdir, f)
      shutil.copy(src, dest)
      # Strip libraries.
      if sys.platform == 'darwin' and f.endswith('.dylib'):
        subprocess.call(['strip', '-x', dest])
      elif (sys.platform.startswith('linux') and
            os.path.splitext(f)[1] in ['.so', '.a']):
        subprocess.call(['strip', '-g', dest])

  # Set up symlinks.
  if sys.platform != 'win32':
    os.symlink('clang', os.path.join(pdir, 'bin', 'clang++'))
    os.symlink('clang', os.path.join(pdir, 'bin', 'clang-cl'))
  if sys.platform == 'darwin':
    os.symlink('libc++.1.dylib', os.path.join(pdir, 'bin', 'libc++.dylib'))
    # Also copy libc++ headers.
    shutil.copytree(os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'include', 'c++'),
                    os.path.join(pdir, 'include', 'c++'))

  # Copy buildlog over.
  shutil.copy('buildlog.txt', pdir)

  # Create archive.
  tar_entries = ['bin', 'lib', 'buildlog.txt']
  if sys.platform == 'darwin':
    tar_entries += ['include']
  with tarfile.open(pdir + '.tgz', 'w:gz') as tar:
    for entry in tar_entries:
      tar.add(os.path.join(pdir, entry), arcname=entry, filter=PrintTarProgress)

  MaybeUpload(args, pdir, platform)

  # Zip up gold plugin on Linux.
  if sys.platform.startswith('linux'):
    shutil.rmtree(golddir, ignore_errors=True)
    os.makedirs(os.path.join(golddir, 'lib'))
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'lib', 'LLVMgold.so'),
                os.path.join(golddir, 'lib'))
    with tarfile.open(golddir + '.tgz', 'w:gz') as tar:
      tar.add(os.path.join(golddir, 'lib'), arcname='lib',
              filter=PrintTarProgress)
    MaybeUpload(args, golddir, platform)

  # FIXME: Warn if the file already exists on the server.


if __name__ == '__main__':
  sys.exit(main())
