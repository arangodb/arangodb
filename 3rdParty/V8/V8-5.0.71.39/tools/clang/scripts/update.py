#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to download prebuilt clang binaries.

It is also used by package.py to build the prebuilt clang binaries."""

import argparse
import distutils.spawn
import glob
import os
import pipes
import re
import shutil
import subprocess
import stat
import sys
import tarfile
import tempfile
import time
import urllib2
import zipfile

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
CLANG_REVISION = '255169'

use_head_revision = 'LLVM_FORCE_HEAD_REVISION' in os.environ
if use_head_revision:
  CLANG_REVISION = 'HEAD'

# This is incremented when pushing a new build of Clang at the same revision.
CLANG_SUB_REVISION=1

PACKAGE_VERSION = "%s-%s" % (CLANG_REVISION, CLANG_SUB_REVISION)

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
CHROME_TOOLS_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'chrometools')
LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts')
COMPILER_RT_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, 'compiler-rt')
CLANG_DIR = os.path.join(LLVM_DIR, 'tools', 'clang')
LLD_DIR = os.path.join(LLVM_DIR, 'tools', 'lld')
# compiler-rt is built as part of the regular LLVM build on Windows to get
# the 64-bit runtime, and out-of-tree elsewhere.
# TODO(thakis): Try to unify this.
if sys.platform == 'win32':
  COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'projects', 'compiler-rt')
else:
  COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'compiler-rt')
LIBCXX_DIR = os.path.join(LLVM_DIR, 'projects', 'libcxx')
LIBCXXABI_DIR = os.path.join(LLVM_DIR, 'projects', 'libcxxabi')
LLVM_BUILD_TOOLS_DIR = os.path.abspath(
    os.path.join(LLVM_DIR, '..', 'llvm-build-tools'))
STAMP_FILE = os.path.normpath(
    os.path.join(LLVM_DIR, '..', 'llvm-build', 'cr_build_revision'))
BINUTILS_DIR = os.path.join(THIRD_PARTY_DIR, 'binutils')
VERSION = '3.8.0'
ANDROID_NDK_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'android_tools', 'ndk')

# URL for pre-built binaries.
CDS_URL = 'https://commondatastorage.googleapis.com/chromium-browser-clang'

LLVM_REPO_URL='https://llvm.org/svn/llvm-project'
if 'LLVM_REPO_URL' in os.environ:
  LLVM_REPO_URL = os.environ['LLVM_REPO_URL']


def DownloadUrl(url, output_file):
  """Download url into output_file."""
  CHUNK_SIZE = 4096
  TOTAL_DOTS = 10
  num_retries = 3
  retry_wait_s = 5  # Doubled at each retry.

  while True:
    try:
      sys.stdout.write('Downloading %s ' % url)
      sys.stdout.flush()
      response = urllib2.urlopen(url)
      total_size = int(response.info().getheader('Content-Length').strip())
      bytes_done = 0
      dots_printed = 0
      while True:
        chunk = response.read(CHUNK_SIZE)
        if not chunk:
          break
        output_file.write(chunk)
        bytes_done += len(chunk)
        num_dots = TOTAL_DOTS * bytes_done / total_size
        sys.stdout.write('.' * (num_dots - dots_printed))
        sys.stdout.flush()
        dots_printed = num_dots
      if bytes_done != total_size:
        raise urllib2.URLError("only got %d of %d bytes" %
                               (bytes_done, total_size))
      print ' Done.'
      return
    except urllib2.URLError as e:
      sys.stdout.write('\n')
      print e
      if num_retries == 0 or isinstance(e, urllib2.HTTPError) and e.code == 404:
        raise e
      num_retries -= 1
      print 'Retrying in %d s ...' % retry_wait_s
      time.sleep(retry_wait_s)
      retry_wait_s *= 2


def EnsureDirExists(path):
  if not os.path.exists(path):
    print "Creating directory %s" % path
    os.makedirs(path)


def DownloadAndUnpack(url, output_dir):
  with tempfile.TemporaryFile() as f:
    DownloadUrl(url, f)
    f.seek(0)
    EnsureDirExists(output_dir)
    if url.endswith('.zip'):
      zipfile.ZipFile(f).extractall(path=output_dir)
    else:
      tarfile.open(mode='r:gz', fileobj=f).extractall(path=output_dir)


def ReadStampFile():
  """Return the contents of the stamp file, or '' if it doesn't exist."""
  try:
    with open(STAMP_FILE, 'r') as f:
      return f.read().rstrip()
  except IOError:
    return ''


def WriteStampFile(s):
  """Write s to the stamp file."""
  EnsureDirExists(os.path.dirname(STAMP_FILE))
  with open(STAMP_FILE, 'w') as f:
    f.write(s)
    f.write('\n')


def GetSvnRevision(svn_repo):
  """Returns current revision of the svn repo at svn_repo."""
  svn_info = subprocess.check_output('svn info ' + svn_repo, shell=True)
  m = re.search(r'Revision: (\d+)', svn_info)
  return m.group(1)


def RmTree(dir):
  """Delete dir."""
  def ChmodAndRetry(func, path, _):
    # Subversion can leave read-only files around.
    if not os.access(path, os.W_OK):
      os.chmod(path, stat.S_IWUSR)
      return func(path)
    raise

  shutil.rmtree(dir, onerror=ChmodAndRetry)


def RmCmakeCache(dir):
  """Delete CMakeCache.txt files under dir recursively."""
  for dirpath, _, files in os.walk(dir):
    if 'CMakeCache.txt' in files:
      os.remove(os.path.join(dirpath, 'CMakeCache.txt'))


def RunCommand(command, msvc_arch=None, env=None, fail_hard=True):
  """Run command and return success (True) or failure; or if fail_hard is
     True, exit on failure.  If msvc_arch is set, runs the command in a
     shell with the msvc tools for that architecture."""

  if msvc_arch and sys.platform == 'win32':
    command = GetVSVersion().SetupScript(msvc_arch) + ['&&'] + command

  # https://docs.python.org/2/library/subprocess.html:
  # "On Unix with shell=True [...] if args is a sequence, the first item
  # specifies the command string, and any additional items will be treated as
  # additional arguments to the shell itself.  That is to say, Popen does the
  # equivalent of:
  #   Popen(['/bin/sh', '-c', args[0], args[1], ...])"
  #
  # We want to pass additional arguments to command[0], not to the shell,
  # so manually join everything into a single string.
  # Annoyingly, for "svn co url c:\path", pipes.quote() thinks that it should
  # quote c:\path but svn can't handle quoted paths on Windows.  Since on
  # Windows follow-on args are passed to args[0] instead of the shell, don't
  # do the single-string transformation there.
  if sys.platform != 'win32':
    command = ' '.join([pipes.quote(c) for c in command])
  print 'Running', command
  if subprocess.call(command, env=env, shell=True) == 0:
    return True
  print 'Failed.'
  if fail_hard:
    sys.exit(1)
  return False


def CopyFile(src, dst):
  """Copy a file from src to dst."""
  shutil.copy(src, dst)
  print "Copying %s to %s" % (src, dst)


def CopyDirectoryContents(src, dst, filename_filter=None):
  """Copy the files from directory src to dst
  with an optional filename filter."""
  dst = os.path.realpath(dst)  # realpath() in case dst ends in /..
  EnsureDirExists(dst)
  for root, _, files in os.walk(src):
    for f in files:
      if filename_filter and not re.match(filename_filter, f):
        continue
      CopyFile(os.path.join(root, f), dst)


def Checkout(name, url, dir):
  """Checkout the SVN module at url into dir. Use name for the log message."""
  print "Checking out %s r%s into '%s'" % (name, CLANG_REVISION, dir)

  command = ['svn', 'checkout', '--force', url + '@' + CLANG_REVISION, dir]
  if RunCommand(command, fail_hard=False):
    return

  if os.path.isdir(dir):
    print "Removing %s." % (dir)
    RmTree(dir)

  print "Retrying."
  RunCommand(command)


def DeleteChromeToolsShim():
  OLD_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'zzz-chrometools')
  shutil.rmtree(OLD_SHIM_DIR, ignore_errors=True)
  shutil.rmtree(CHROME_TOOLS_SHIM_DIR, ignore_errors=True)


def CreateChromeToolsShim():
  """Hooks the Chrome tools into the LLVM build.

  Several Chrome tools have dependencies on LLVM/Clang libraries. The LLVM build
  detects implicit tools in the tools subdirectory, so this helper install a
  shim CMakeLists.txt that forwards to the real directory for the Chrome tools.

  Note that the shim directory name intentionally has no - or _. The implicit
  tool detection logic munges them in a weird way."""
  assert not any(i in os.path.basename(CHROME_TOOLS_SHIM_DIR) for i in '-_')
  os.mkdir(CHROME_TOOLS_SHIM_DIR)
  with file(os.path.join(CHROME_TOOLS_SHIM_DIR, 'CMakeLists.txt'), 'w') as f:
    f.write('# Automatically generated by tools/clang/scripts/update.py. ' +
            'Do not edit.\n')
    f.write('# Since tools/clang is located in another directory, use the \n')
    f.write('# two arg version to specify where build artifacts go. CMake\n')
    f.write('# disallows reuse of the same binary dir for multiple source\n')
    f.write('# dirs, so the build artifacts need to go into a subdirectory.\n')
    f.write('# dirs, so the build artifacts need to go into a subdirectory.\n')
    f.write('if (CHROMIUM_TOOLS_SRC)\n')
    f.write('  add_subdirectory(${CHROMIUM_TOOLS_SRC} ' +
              '${CMAKE_CURRENT_BINARY_DIR}/a)\n')
    f.write('endif (CHROMIUM_TOOLS_SRC)\n')


def MaybeDownloadHostGcc(args):
  """Downloads gcc 4.8.2 if needed and makes sure args.gcc_toolchain is set."""
  if not sys.platform.startswith('linux') or args.gcc_toolchain:
    return

  if subprocess.check_output(['gcc', '-dumpversion']).rstrip() < '4.7.0':
    # We need a newer gcc version.
    gcc_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'gcc482precise')
    if not os.path.exists(gcc_dir):
      print 'Downloading pre-built GCC 4.8.2...'
      DownloadAndUnpack(
          CDS_URL + '/tools/gcc482precise.tgz', LLVM_BUILD_TOOLS_DIR)
    args.gcc_toolchain = gcc_dir
  else:
    # Always set gcc_toolchain; llvm-symbolizer needs the bundled libstdc++.
    args.gcc_toolchain = \
        os.path.dirname(os.path.dirname(distutils.spawn.find_executable('gcc')))


def AddCMakeToPath():
  """Download CMake and add it to PATH."""
  if sys.platform == 'win32':
    zip_name = 'cmake-3.2.2-win32-x86.zip'
    cmake_dir = os.path.join(LLVM_BUILD_TOOLS_DIR,
                             'cmake-3.2.2-win32-x86', 'bin')
  else:
    suffix = 'Darwin' if sys.platform == 'darwin' else 'Linux'
    zip_name = 'cmake322_%s.tgz' % suffix
    cmake_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'cmake322', 'bin')
  if not os.path.exists(cmake_dir):
    DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
  os.environ['PATH'] = cmake_dir + os.pathsep + os.environ.get('PATH', '')

vs_version = None
def GetVSVersion():
  global vs_version
  if vs_version:
    return vs_version

  # Try using the toolchain in depot_tools.
  # This sets environment variables used by SelectVisualStudioVersion below.
  sys.path.append(os.path.join(CHROMIUM_DIR, 'build'))
  import vs_toolchain
  vs_toolchain.SetEnvironmentAndGetRuntimeDllDirs()

  # Use gyp to find the MSVS installation, either in depot_tools as per above,
  # or a system-wide installation otherwise.
  sys.path.append(os.path.join(CHROMIUM_DIR, 'tools', 'gyp', 'pylib'))
  import gyp.MSVSVersion
  vs_version = gyp.MSVSVersion.SelectVisualStudioVersion('2013')
  return vs_version


def UpdateClang(args):
  print 'Updating Clang to %s...' % PACKAGE_VERSION

  need_gold_plugin = 'LLVM_DOWNLOAD_GOLD_PLUGIN' in os.environ or (
      sys.platform.startswith('linux') and
      'buildtype=Official' in os.environ.get('GYP_DEFINES', '') and
      'branding=Chrome' in os.environ.get('GYP_DEFINES', ''))

  if ReadStampFile() == PACKAGE_VERSION and not args.force_local_build:
    print 'Clang is already up to date.'
    if not need_gold_plugin or os.path.exists(
        os.path.join(LLVM_BUILD_DIR, "lib/LLVMgold.so")):
      return 0

  # Reset the stamp file in case the build is unsuccessful.
  WriteStampFile('')

  if not args.force_local_build:
    cds_file = "clang-%s.tgz" %  PACKAGE_VERSION
    if sys.platform == 'win32':
      cds_full_url = CDS_URL + '/Win/' + cds_file
    elif sys.platform == 'darwin':
      cds_full_url = CDS_URL + '/Mac/' + cds_file
    else:
      assert sys.platform.startswith('linux')
      cds_full_url = CDS_URL + '/Linux_x64/' + cds_file

    print 'Downloading prebuilt clang'
    if os.path.exists(LLVM_BUILD_DIR):
      RmTree(LLVM_BUILD_DIR)
    try:
      DownloadAndUnpack(cds_full_url, LLVM_BUILD_DIR)
      print 'clang %s unpacked' % PACKAGE_VERSION
      # Download the gold plugin if requested to by an environment variable.
      # This is used by the CFI ClusterFuzz bot, and it's required for official
      # builds on linux.
      if need_gold_plugin:
        RunCommand(['python', CHROMIUM_DIR+'/build/download_gold_plugin.py'])
      WriteStampFile(PACKAGE_VERSION)
      return 0
    except urllib2.URLError:
      print 'Failed to download prebuilt clang %s' % cds_file
      print 'Use --force-local-build if you want to build locally.'
      print 'Exiting.'
      return 1

  if args.with_android and not os.path.exists(ANDROID_NDK_DIR):
    print 'Android NDK not found at ' + ANDROID_NDK_DIR
    print 'The Android NDK is needed to build a Clang whose -fsanitize=address'
    print 'works on Android. See '
    print 'http://code.google.com/p/chromium/wiki/AndroidBuildInstructions'
    print 'for how to install the NDK, or pass --without-android.'
    return 1

  MaybeDownloadHostGcc(args)
  AddCMakeToPath()

  DeleteChromeToolsShim()

  Checkout('LLVM', LLVM_REPO_URL + '/llvm/trunk', LLVM_DIR)
  Checkout('Clang', LLVM_REPO_URL + '/cfe/trunk', CLANG_DIR)
  if sys.platform == 'win32':
    Checkout('LLD', LLVM_REPO_URL + '/lld/trunk', LLD_DIR)
  Checkout('compiler-rt', LLVM_REPO_URL + '/compiler-rt/trunk', COMPILER_RT_DIR)
  if sys.platform == 'darwin':
    # clang needs a libc++ checkout, else -stdlib=libc++ won't find includes
    # (i.e. this is needed for bootstrap builds).
    Checkout('libcxx', LLVM_REPO_URL + '/libcxx/trunk', LIBCXX_DIR)
    # While we're bundling our own libc++ on OS X, we need to compile libc++abi
    # into it too (since OS X 10.6 doesn't have libc++abi.dylib either).
    Checkout('libcxxabi', LLVM_REPO_URL + '/libcxxabi/trunk', LIBCXXABI_DIR)

  cc, cxx = None, None
  libstdcpp = None
  if args.gcc_toolchain:  # This option is only used on Linux.
    # Use the specified gcc installation for building.
    cc = os.path.join(args.gcc_toolchain, 'bin', 'gcc')
    cxx = os.path.join(args.gcc_toolchain, 'bin', 'g++')

    if not os.access(cc, os.X_OK):
      print 'Invalid --gcc-toolchain: "%s"' % args.gcc_toolchain
      print '"%s" does not appear to be valid.' % cc
      return 1

    # Set LD_LIBRARY_PATH to make auxiliary targets (tablegen, bootstrap
    # compiler, etc.) find the .so.
    libstdcpp = subprocess.check_output(
        [cxx, '-print-file-name=libstdc++.so.6']).rstrip()
    os.environ['LD_LIBRARY_PATH'] = os.path.dirname(libstdcpp)

  cflags = cxxflags = ldflags = []

  # LLVM uses C++11 starting in llvm 3.5. On Linux, this means libstdc++4.7+ is
  # needed, on OS X it requires libc++. clang only automatically links to libc++
  # when targeting OS X 10.9+, so add stdlib=libc++ explicitly so clang can run
  # on OS X versions as old as 10.7.
  # TODO(thakis): Some bots are still on 10.6 (nacl...), so for now bundle
  # libc++.dylib.  Remove this once all bots are on 10.7+, then use
  # -DLLVM_ENABLE_LIBCXX=ON and change deployment_target to 10.7.
  deployment_target = ''

  if sys.platform == 'darwin':
    # When building on 10.9, /usr/include usually doesn't exist, and while
    # Xcode's clang automatically sets a sysroot, self-built clangs don't.
    cflags = ['-isysroot', subprocess.check_output(
        ['xcrun', '--show-sdk-path']).rstrip()]
    cxxflags = ['-stdlib=libc++', '-nostdinc++',
                '-I' + os.path.join(LIBCXX_DIR, 'include')] + cflags
    if args.bootstrap:
      deployment_target = '10.6'

  base_cmake_args = ['-GNinja',
                     '-DCMAKE_BUILD_TYPE=Release',
                     '-DLLVM_ENABLE_ASSERTIONS=ON',
                     '-DLLVM_ENABLE_THREADS=OFF',
                     ]

  if args.bootstrap:
    print 'Building bootstrap compiler'
    EnsureDirExists(LLVM_BOOTSTRAP_DIR)
    os.chdir(LLVM_BOOTSTRAP_DIR)
    bootstrap_args = base_cmake_args + [
        '-DLLVM_TARGETS_TO_BUILD=host',
        '-DCMAKE_INSTALL_PREFIX=' + LLVM_BOOTSTRAP_INSTALL_DIR,
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        ]
    if cc is not None:  bootstrap_args.append('-DCMAKE_C_COMPILER=' + cc)
    if cxx is not None: bootstrap_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
    RmCmakeCache('.')
    RunCommand(['cmake'] + bootstrap_args + [LLVM_DIR], msvc_arch='x64')
    RunCommand(['ninja'], msvc_arch='x64')
    if args.run_tests:
      RunCommand(['ninja', 'check-all'], msvc_arch='x64')
    RunCommand(['ninja', 'install'], msvc_arch='x64')
    if args.gcc_toolchain:
      # Copy that gcc's stdlibc++.so.6 to the build dir, so the bootstrap
      # compiler can start.
      CopyFile(libstdcpp, os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'lib'))

    if sys.platform == 'win32':
      cc = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang-cl.exe')
      cxx = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang-cl.exe')
      # CMake has a hard time with backslashes in compiler paths:
      # https://stackoverflow.com/questions/13050827
      cc = cc.replace('\\', '/')
      cxx = cxx.replace('\\', '/')
    else:
      cc = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang')
      cxx = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang++')

    if args.gcc_toolchain:
      # Tell the bootstrap compiler to use a specific gcc prefix to search
      # for standard library headers and shared object files.
      cflags = ['--gcc-toolchain=' + args.gcc_toolchain]
      cxxflags = ['--gcc-toolchain=' + args.gcc_toolchain]
    print 'Building final compiler'

  if sys.platform == 'darwin':
    # Build libc++.dylib while some bots are still on OS X 10.6.
    libcxxbuild = os.path.join(LLVM_BUILD_DIR, 'libcxxbuild')
    if os.path.isdir(libcxxbuild):
      RmTree(libcxxbuild)
    libcxxflags = ['-O3', '-std=c++11', '-fstrict-aliasing']

    # libcxx and libcxxabi both have a file stdexcept.cpp, so put their .o files
    # into different subdirectories.
    os.makedirs(os.path.join(libcxxbuild, 'libcxx'))
    os.chdir(os.path.join(libcxxbuild, 'libcxx'))
    RunCommand(['c++', '-c'] + cxxflags + libcxxflags +
                glob.glob(os.path.join(LIBCXX_DIR, 'src', '*.cpp')))

    os.makedirs(os.path.join(libcxxbuild, 'libcxxabi'))
    os.chdir(os.path.join(libcxxbuild, 'libcxxabi'))
    RunCommand(['c++', '-c'] + cxxflags + libcxxflags +
               glob.glob(os.path.join(LIBCXXABI_DIR, 'src', '*.cpp')) +
               ['-I' + os.path.join(LIBCXXABI_DIR, 'include')])

    os.chdir(libcxxbuild)
    libdir = os.path.join(LIBCXX_DIR, 'lib')
    RunCommand(['cc'] + glob.glob('libcxx/*.o') + glob.glob('libcxxabi/*.o') +
        ['-o', 'libc++.1.dylib', '-dynamiclib', '-nodefaultlibs',
         '-current_version', '1', '-compatibility_version', '1', '-lSystem',
         '-install_name', '@executable_path/libc++.dylib',
         '-Wl,-unexported_symbols_list,' + libdir + '/libc++unexp.exp',
         '-Wl,-force_symbols_not_weak_list,' + libdir + '/notweak.exp',
         '-Wl,-force_symbols_weak_list,' + libdir + '/weak.exp'])
    if os.path.exists('libc++.dylib'):
      os.remove('libc++.dylib')
    os.symlink('libc++.1.dylib', 'libc++.dylib')
    ldflags += ['-stdlib=libc++', '-L' + libcxxbuild]

    if args.bootstrap:
      # Now that the libc++ headers have been installed and libc++.dylib is
      # built, delete the libc++ checkout again so that it's not part of the
      # main build below -- the libc++(abi) tests don't pass on OS X in
      # bootstrap builds (http://llvm.org/PR24068)
      RmTree(LIBCXX_DIR)
      RmTree(LIBCXXABI_DIR)
      cxxflags = ['-stdlib=libc++', '-nostdinc++',
                  '-I' + os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR,
                                      'include/c++/v1')
                  ] + cflags

  # Build clang.
  binutils_incdir = ''
  if sys.platform.startswith('linux'):
    binutils_incdir = os.path.join(BINUTILS_DIR, 'Linux_x64/Release/include')

  # If building at head, define a macro that plugins can use for #ifdefing
  # out code that builds at head, but not at CLANG_REVISION or vice versa.
  if use_head_revision:
    cflags += ['-DLLVM_FORCE_HEAD_REVISION']
    cxxflags += ['-DLLVM_FORCE_HEAD_REVISION']

  CreateChromeToolsShim()

  deployment_env = None
  if deployment_target:
    deployment_env = os.environ.copy()
    deployment_env['MACOSX_DEPLOYMENT_TARGET'] = deployment_target

  cmake_args = []
  # TODO(thakis): Unconditionally append this to base_cmake_args instead once
  # compiler-rt can build with clang-cl on Windows (http://llvm.org/PR23698)
  cc_args = base_cmake_args if sys.platform != 'win32' else cmake_args
  if cc is not None:  cc_args.append('-DCMAKE_C_COMPILER=' + cc)
  if cxx is not None: cc_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
  cmake_args += base_cmake_args + [
      '-DLLVM_BINUTILS_INCDIR=' + binutils_incdir,
      '-DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly',
      '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
      '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
      '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_INSTALL_PREFIX=' + LLVM_BUILD_DIR,
      '-DCHROMIUM_TOOLS_SRC=%s' % os.path.join(CHROMIUM_DIR, 'tools', 'clang'),
      '-DCHROMIUM_TOOLS=%s' % ';'.join(args.tools)]

  EnsureDirExists(LLVM_BUILD_DIR)
  os.chdir(LLVM_BUILD_DIR)
  RmCmakeCache('.')
  RunCommand(['cmake'] + cmake_args + [LLVM_DIR],
             msvc_arch='x64', env=deployment_env)

  if args.gcc_toolchain:
    # Copy in the right stdlibc++.so.6 so clang can start.
    if not os.path.exists(os.path.join(LLVM_BUILD_DIR, 'lib')):
      os.mkdir(os.path.join(LLVM_BUILD_DIR, 'lib'))
    libstdcpp = subprocess.check_output(
        [cxx] + cxxflags + ['-print-file-name=libstdc++.so.6']).rstrip()
    CopyFile(libstdcpp, os.path.join(LLVM_BUILD_DIR, 'lib'))

  # TODO(thakis): Remove "-d explain" once http://crbug.com/569337 is fixed.
  RunCommand(['ninja', '-d', 'explain'], msvc_arch='x64')

  if args.tools:
    # If any Chromium tools were built, install those now.
    RunCommand(['ninja', 'cr-install'], msvc_arch='x64')

  if sys.platform == 'darwin':
    CopyFile(os.path.join(libcxxbuild, 'libc++.1.dylib'),
             os.path.join(LLVM_BUILD_DIR, 'bin'))
    # See http://crbug.com/256342
    RunCommand(['strip', '-x', os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')])
  elif sys.platform.startswith('linux'):
    RunCommand(['strip', os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')])

  # Do an out-of-tree build of compiler-rt.
  # On Windows, this is used to get the 32-bit ASan run-time.
  # TODO(hans): Remove once the regular build above produces this.
  # On Mac and Linux, this is used to get the regular 64-bit run-time.
  # Do a clobbered build due to cmake changes.
  if os.path.isdir(COMPILER_RT_BUILD_DIR):
    RmTree(COMPILER_RT_BUILD_DIR)
  os.makedirs(COMPILER_RT_BUILD_DIR)
  os.chdir(COMPILER_RT_BUILD_DIR)
  # TODO(thakis): Add this once compiler-rt can build with clang-cl (see
  # above).
  #if args.bootstrap and sys.platform == 'win32':
    # The bootstrap compiler produces 64-bit binaries by default.
    #cflags += ['-m32']
    #cxxflags += ['-m32']
  compiler_rt_args = base_cmake_args + [
      '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
      '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags)]
  if sys.platform != 'win32':
    compiler_rt_args += ['-DLLVM_CONFIG_PATH=' +
                         os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-config'),
                        '-DSANITIZER_MIN_OSX_VERSION="10.7"']
  # compiler-rt is part of the llvm checkout on Windows but a stand-alone
  # directory elsewhere, see the TODO above COMPILER_RT_DIR.
  RmCmakeCache('.')
  RunCommand(['cmake'] + compiler_rt_args +
             [LLVM_DIR if sys.platform == 'win32' else COMPILER_RT_DIR],
             msvc_arch='x86', env=deployment_env)
  RunCommand(['ninja', 'compiler-rt'], msvc_arch='x86')

  # Copy select output to the main tree.
  # TODO(hans): Make this (and the .gypi and .isolate files) version number
  # independent.
  if sys.platform == 'win32':
    platform = 'windows'
  elif sys.platform == 'darwin':
    platform = 'darwin'
  else:
    assert sys.platform.startswith('linux')
    platform = 'linux'
  asan_rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', platform)
  if sys.platform == 'win32':
    # TODO(thakis): This too is due to compiler-rt being part of the checkout
    # on Windows, see TODO above COMPILER_RT_DIR.
    asan_rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', 'clang',
                                       VERSION, 'lib', platform)
  asan_rt_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                     VERSION, 'lib', platform)
  # Blacklists:
  CopyDirectoryContents(os.path.join(asan_rt_lib_src_dir, '..', '..'),
                        os.path.join(asan_rt_lib_dst_dir, '..', '..'),
                        r'^.*blacklist\.txt$')
  # Headers:
  if sys.platform != 'win32':
    CopyDirectoryContents(
        os.path.join(COMPILER_RT_BUILD_DIR, 'include/sanitizer'),
        os.path.join(LLVM_BUILD_DIR, 'lib/clang', VERSION, 'include/sanitizer'))
  # Static and dynamic libraries:
  CopyDirectoryContents(asan_rt_lib_src_dir, asan_rt_lib_dst_dir)
  if sys.platform == 'darwin':
    for dylib in glob.glob(os.path.join(asan_rt_lib_dst_dir, '*.dylib')):
      # Fix LC_ID_DYLIB for the ASan dynamic libraries to be relative to
      # @executable_path.
      # TODO(glider): this is transitional. We'll need to fix the dylib
      # name either in our build system, or in Clang. See also
      # http://crbug.com/344836.
      subprocess.call(['install_name_tool', '-id',
                       '@executable_path/' + os.path.basename(dylib), dylib])


  if sys.platform == 'win32':
    # Make an extra copy of the sanitizer headers, to be put on the include path
    # of the fallback compiler.
    sanitizer_include_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                         VERSION, 'include', 'sanitizer')
    aux_sanitizer_include_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                             VERSION, 'include_sanitizer',
                                             'sanitizer')
    EnsureDirExists(aux_sanitizer_include_dir)
    for _, _, files in os.walk(sanitizer_include_dir):
      for f in files:
        CopyFile(os.path.join(sanitizer_include_dir, f),
                 aux_sanitizer_include_dir)

  if args.with_android:
    make_toolchain = os.path.join(
        ANDROID_NDK_DIR, 'build', 'tools', 'make-standalone-toolchain.sh')
    for target_arch in ['aarch64', 'arm', 'i686']:
      # Make standalone Android toolchain for target_arch.
      toolchain_dir = os.path.join(
          LLVM_BUILD_DIR, 'android-toolchain-' + target_arch)
      RunCommand([
          make_toolchain,
          '--platform=android-' + ('21' if target_arch == 'aarch64' else '19'),
          '--install-dir="%s"' % toolchain_dir,
          '--system=linux-x86_64',
          '--stl=stlport',
          '--toolchain=' + {
              'aarch64': 'aarch64-linux-android-4.9',
              'arm': 'arm-linux-androideabi-4.9',
              'i686': 'x86-4.9',
          }[target_arch]])
      # Android NDK r9d copies a broken unwind.h into the toolchain, see
      # http://crbug.com/357890
      for f in glob.glob(os.path.join(toolchain_dir, 'include/c++/*/unwind.h')):
        os.remove(f)

      # Build ASan runtime for Android in a separate build tree.
      build_dir = os.path.join(LLVM_BUILD_DIR, 'android-' + target_arch)
      if not os.path.exists(build_dir):
        os.mkdir(os.path.join(build_dir))
      os.chdir(build_dir)
      cflags = ['--target=%s-linux-androideabi' % target_arch,
                '--sysroot=%s/sysroot' % toolchain_dir,
                '-B%s' % toolchain_dir]
      android_args = base_cmake_args + [
        '-DCMAKE_C_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_CXX_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang++'),
        '-DLLVM_CONFIG_PATH=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-config'),
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cflags),
        '-DANDROID=1']
      RmCmakeCache('.')
      RunCommand(['cmake'] + android_args + [COMPILER_RT_DIR])
      RunCommand(['ninja', 'libclang_rt.asan-%s-android.so' % target_arch])

      # And copy it into the main build tree.
      runtime = 'libclang_rt.asan-%s-android.so' % target_arch
      for root, _, files in os.walk(build_dir):
        if runtime in files:
          shutil.copy(os.path.join(root, runtime), asan_rt_lib_dst_dir)

  # Run tests.
  if args.run_tests or use_head_revision:
    os.chdir(LLVM_BUILD_DIR)
    RunCommand(['ninja', 'cr-check-all'], msvc_arch='x64')
  if args.run_tests:
    os.chdir(LLVM_BUILD_DIR)
    RunCommand(['ninja', 'check-all'], msvc_arch='x64')

  WriteStampFile(PACKAGE_VERSION)
  print 'Clang update was successful.'
  return 0


def main():
  parser = argparse.ArgumentParser(description='Build Clang.')
  parser.add_argument('--bootstrap', action='store_true',
                      help='first build clang with CC, then with itself.')
  parser.add_argument('--if-needed', action='store_true',
                      help="run only if the script thinks clang is needed")
  parser.add_argument('--force-local-build', action='store_true',
                      help="don't try to download prebuild binaries")
  parser.add_argument('--gcc-toolchain', help='set the version for which gcc '
                      'version be used for building; --gcc-toolchain=/opt/foo '
                      'picks /opt/foo/bin/gcc')
  parser.add_argument('--print-revision', action='store_true',
                      help='print current clang revision and exit.')
  parser.add_argument('--print-clang-version', action='store_true',
                      help='print current clang version (e.g. x.y.z) and exit.')
  parser.add_argument('--run-tests', action='store_true',
                      help='run tests after building; only for local builds')
  parser.add_argument('--tools', nargs='*',
                      help='select which chrome tools to build',
                      default=['plugins', 'blink_gc_plugin'])
  parser.add_argument('--without-android', action='store_false',
                      help='don\'t build Android ASan runtime (linux only)',
                      dest='with_android',
                      default=sys.platform.startswith('linux'))
  args = parser.parse_args()

  if args.if_needed:
    is_clang_required = False
    # clang is always used on Mac and Linux.
    if sys.platform == 'darwin' or sys.platform.startswith('linux'):
      is_clang_required = True
    # clang requested via $GYP_DEFINES.
    if re.search(r'\b(clang|asan|lsan|msan|tsan)=1',
                 os.environ.get('GYP_DEFINES', '')):
      is_clang_required = True
    # clang previously downloaded, keep it up-to-date.
    # If you don't want this, delete third_party/llvm-build on your machine.
    if os.path.isdir(LLVM_BUILD_DIR):
      is_clang_required = True
    if not is_clang_required:
      return 0
    if re.search(r'\b(make_clang_dir)=', os.environ.get('GYP_DEFINES', '')):
      print 'Skipping Clang update (make_clang_dir= was set in GYP_DEFINES).'
      return 0

  if use_head_revision:
    # TODO(hans): Remove after the next roll.
    global VERSION
    VERSION = '3.9.0'

  global CLANG_REVISION, PACKAGE_VERSION
  if args.print_revision:
    if use_head_revision:
      print GetSvnRevision(LLVM_DIR)
    else:
      print PACKAGE_VERSION
    return 0

  if args.print_clang_version:
    sys.stdout.write(VERSION)
    return 0

  # Don't buffer stdout, so that print statements are immediately flushed.
  # Do this only after --print-revision has been handled, else we'll get
  # an error message when this script is run from gn for some reason.
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

  if use_head_revision:
    # Use a real revision number rather than HEAD to make sure that the stamp
    # file logic works.
    CLANG_REVISION = GetSvnRevision(LLVM_REPO_URL)
    PACKAGE_VERSION = CLANG_REVISION + '-0'

    args.force_local_build = True
    if 'OS=android' not in os.environ.get('GYP_DEFINES', ''):
      # Only build the Android ASan rt on ToT bots when targetting Android.
      args.with_android = False

  return UpdateClang(args)


if __name__ == '__main__':
  sys.exit(main())
