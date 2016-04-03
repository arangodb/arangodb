# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Utilities to work with importable python zip packages."""

import atexit
import collections
import cStringIO as StringIO
import hashlib
import os
import pkgutil
import re
import sys
import tempfile
import threading
import zipfile
import zipimport


# Glob patterns for files to exclude from a package by default.
EXCLUDE_LIST = (
  # Ignore hidden files (including .svn and .git).
  r'\..*',

  # Ignore precompiled python files since they depend on python version and we
  # don't want zip package to be version-depended.
  r'.*\.pyc$',
  r'.*\.pyo$',
)


# Temporary files extracted by extract_resource. Removed in atexit hook.
_extracted_files = []
_extracted_files_lock = threading.Lock()


class ZipPackageError(RuntimeError):
  """Failed to create a zip package."""


class ZipPackage(object):
  """A set of files that can be zipped to file on disk or into memory buffer.

  Usage:
    package = ZipPackage(root)
    package.add_file('some_file.py', '__main__.py')
    package.add_directory('some_directory')
    package.add_buffer('generated.py', 'any string here')

    buf = package.zip_into_buffer()
    package.zip_into_file('my_zip.zip')
  """

  _FileRef = collections.namedtuple('_FileRef', ['abs_path'])
  _BufferRef = collections.namedtuple('_BufferRef', ['buffer'])

  def __init__(self, root):
    """Initializes new empty ZipPackage.

    All files added to the package should live under the |root|. It will also
    be used when calculating relative paths of files in the package.

    |root| must be an absolute path.
    """
    assert os.path.isabs(root), root
    self.root = root.rstrip(os.sep) + os.sep
    self._items = {}

  @property
  def files(self):
    """Files added to the package as a list of relative paths in zip."""
    return self._items.keys()

  def add_file(self, absolute_path, archive_path=None):
    """Adds a single file to the package.

    |archive_path| is a relative path in archive for this file, by default it's
    equal to |absolute_path| taken relative to |root|. In that case
    |absolute_path| must be in a |root| subtree.

    If |archive_path| is given, |absolute_path| can point to any file.
    """
    assert os.path.isabs(absolute_path), absolute_path
    absolute_path = os.path.normpath(absolute_path)
    # If |archive_path| is not given, ensure that |absolute_path| is under root.
    if not archive_path and not absolute_path.startswith(self.root):
      raise ZipPackageError(
          'Path %s is not inside root %s' % (absolute_path, self.root))
    if not os.path.exists(absolute_path):
      raise ZipPackageError('No such file: %s' % absolute_path)
    if not os.path.isfile(absolute_path):
      raise ZipPackageError('Object %s is not a regular file' % absolute_path)
    archive_path = archive_path or absolute_path[len(self.root):]
    self._add_entry(archive_path, ZipPackage._FileRef(absolute_path))

  def add_python_file(self, absolute_path, archive_path=None):
    """Adds a single python file to the package.

    Recognizes *.pyc files and adds corresponding *.py file instead.
    """
    base, ext = os.path.splitext(absolute_path)
    if ext in ('.pyc', '.pyo'):
      absolute_path = base + '.py'
    elif ext != '.py':
      raise ZipPackageError('Not a python file: %s' % absolute_path)
    self.add_file(absolute_path, archive_path)

  def add_directory(self, absolute_path, archive_path=None,
                    exclude=EXCLUDE_LIST):
    """Recursively adds all files from given directory to the package.

    |archive_path| is a relative path in archive for this directory, by default
    it's equal to |absolute_path| taken relative to |root|. In that case
    |absolute_path| must be in |root| subtree.

    If |archive_path| is given, |absolute_path| can point to any directory.

    |exclude| defines a list of regular expressions for file names to exclude
    from the package.

    Only non-empty directories will be actually added to the package.
    """
    assert os.path.isabs(absolute_path), absolute_path
    absolute_path = os.path.normpath(absolute_path).rstrip(os.sep) + os.sep
    # If |archive_path| is not given, ensure that |path| is under root.
    if not archive_path and not absolute_path.startswith(self.root):
      raise ZipPackageError(
          'Path %s is not inside root %s' % (absolute_path, self.root))
    if not os.path.exists(absolute_path):
      raise ZipPackageError('No such directory: %s' % absolute_path)
    if not os.path.isdir(absolute_path):
      raise ZipPackageError('Object %s is not a directory' % absolute_path)

    # Precompile regular expressions.
    exclude_regexps = [re.compile(r) for r in exclude]
    # Returns True if |name| should be excluded from the package.
    should_exclude = lambda name: any(r.match(name) for r in exclude_regexps)

    archive_path = archive_path or absolute_path[len(self.root):]
    for cur_dir, dirs, files in os.walk(absolute_path):
      # Add all non-excluded files.
      for name in files:
        if not should_exclude(name):
          absolute = os.path.join(cur_dir, name)
          relative = absolute[len(absolute_path):]
          assert absolute.startswith(absolute_path)
          self.add_file(absolute, os.path.join(archive_path, relative))
      # Remove excluded directories from enumeration.
      for name in [d for d in dirs if should_exclude(d)]:
        dirs.remove(name)

  def add_buffer(self, archive_path, buf):
    """Adds a contents of the given string |buf| to the package as a file.

    |archive_path| is a path in archive for this file.
    """
    # Only 'str' is allowed here, no 'unicode'
    assert isinstance(buf, str)
    self._add_entry(archive_path, ZipPackage._BufferRef(buf))

  def zip_into_buffer(self, compress=True):
    """Zips added files into in-memory zip file and returns it as str."""
    stream = StringIO.StringIO()
    try:
      self._zip_into_stream(stream, compress)
      return stream.getvalue()
    finally:
      stream.close()

  def zip_into_file(self, path, compress=True):
    """Zips added files into a file on disk."""
    with open(path, 'wb') as stream:
      self._zip_into_stream(stream, compress)

  def _add_entry(self, archive_path, ref):
    """Adds new zip package entry."""
    # Always use forward slashes in zip.
    archive_path = archive_path.replace(os.sep, '/')
    # Ensure there are no suspicious components in the path.
    assert not any(p in ('', '.', '..') for p in archive_path.split('/'))
    # Ensure there's no file overwrites.
    if archive_path in self._items:
      raise ZipPackageError('Duplicated entry: %s' % archive_path)
    self._items[archive_path] = ref

  def _zip_into_stream(self, stream, compress):
    """Zips files added so far into some output stream.

    Some measures are taken to guarantee that final zip depends only on the
    content of added files:
      * File modification time is not stored.
      * Entries are sorted by file name in archive.
    """
    compression = zipfile.ZIP_DEFLATED if compress else zipfile.ZIP_STORED
    zip_file = zipfile.ZipFile(stream, 'w', compression)
    try:
      for archive_path in sorted(self._items):
        ref = self._items[archive_path]
        info = zipfile.ZipInfo(filename=archive_path)
        info.compress_type = compression
        info.create_system = 3
        if isinstance(ref, ZipPackage._FileRef):
          info.external_attr = (os.stat(ref.abs_path)[0] & 0xFFFF) << 16L
          with open(ref.abs_path, 'rb') as f:
            buf = f.read()
        elif isinstance(ref, ZipPackage._BufferRef):
          buf = ref.buffer
        else:
          assert False, 'Unexpected type %s' % ref
        zip_file.writestr(info, buf)
    finally:
      zip_file.close()


def get_module_zip_archive(module):
  """Given a module, returns path to a zip package that contains it or None."""
  loader = pkgutil.get_loader(module)
  if not isinstance(loader, zipimport.zipimporter):
    return None
  # 'archive' property is documented only for python 2.7, but it appears to be
  # there at least since python 2.5.2.
  return loader.archive


def is_zipped_module(module):
  """True if given module was loaded from a zip package."""
  return bool(get_module_zip_archive(module))


def get_main_script_path():
  """If running from zip returns path to a zip file, else path to __main__.

  Basically returns path to a file passed to python for execution
  as in 'python <main_script>' considering a case of executable zip package.

  Returns path relative to a current directory of when process was started.
  """
  # If running from interactive console __file__ is not defined.
  main = sys.modules['__main__']
  return get_module_zip_archive(main) or getattr(main, '__file__', None)


def _write_temp_data(name, data, temp_dir):
  """Writes content-addressed file in `temp_dir` if relevant."""
  filename = '%s-%s' % (hashlib.sha1(data).hexdigest(), name)
  filepath = os.path.join(temp_dir, filename)
  if os.path.isfile(filepath):
    with open(filepath, 'rb') as f:
      if f.read() == data:
        # It already exists.
        return filepath
    # It's different, can't use it.
    return None

  try:
    fd = os.open(filepath, os.O_WRONLY|os.O_CREAT|os.O_EXCL, 0600)
    with os.fdopen(fd, 'wb') as f:
      f.write(data)
    return filepath
  except (IOError, OSError):
    return None


def extract_resource(package, resource, temp_dir=None):
  """Returns real file system path to a |resource| file from a |package|.

  If it's inside a zip package, will extract it first into a file. Such file is
  readable and writable only by the creating user ID.

  Arguments:
    package: is a python module object that represents a package.
    resource: should be a relative filename, using '/'' as the path separator.
    temp_dir: if set, it will extra the file in this directory with the filename
        being the hash of the content. Otherwise, it uses tempfile.mkstemp().

  Raises ValueError if no such resource.
  """
  # For regular non-zip packages just construct an absolute path.
  if not is_zipped_module(package):
    # Package's __file__ attribute is always an absolute path.
    path = os.path.join(os.path.dirname(package.__file__),
        resource.replace('/', os.sep))
    if not os.path.exists(path):
      raise ValueError('No such resource in %s: %s' % (package, resource))
    return path

  # For zipped packages extract the resource into a temp file.
  data = pkgutil.get_data(package.__name__, resource)
  if data is None:
    raise ValueError('No such resource in zipped %s: %s' % (package, resource))

  if temp_dir:
    filepath = _write_temp_data(os.path.basename(resource), data, temp_dir)
    if filepath:
      return filepath

  fd, filepath = tempfile.mkstemp(
      prefix=u'.zip_pkg-',
      suffix=u'-' + os.path.basename(resource),
      dir=temp_dir)
  with os.fdopen(fd, 'wb') as stream:
    stream.write(data)

  # Register it for removal when process dies.
  with _extracted_files_lock:
    _extracted_files.append(filepath)
    # First extracted file -> register atexit hook that cleans them all.
    if len(_extracted_files) == 1:
      atexit.register(cleanup_extracted_resources)

  return filepath


def cleanup_extracted_resources():
  """Removes all temporary files created by extract_resource.

  Executed as atexit hook.
  """
  with _extracted_files_lock:
    while _extracted_files:
      try:
        os.remove(_extracted_files.pop())
      except OSError:
        pass


def generate_version():
  """Generates the sha-1 based on the content of this zip.

  It is hashing the content of the zip, not the compressed bits. The compression
  has other side effects that kicks in, like zlib's library version, compression
  level, order in which the files were specified, etc.
  """
  assert is_zipped_module(sys.modules['__main__'])
  h = hashlib.sha1()
  with zipfile.ZipFile(get_main_script_path(), 'r') as z:
    for name in sorted(z.namelist()):
      with z.open(name) as f:
        h.update(str(len(name)))
        h.update(name)
        content = f.read()
        h.update(str(len(content)))
        h.update(content)
  return h.hexdigest()
