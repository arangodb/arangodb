# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Understands .isolated files and can do local operations on them."""

import hashlib
import json
import logging
import os
import re
import stat
import sys

from utils import file_path
from utils import fs
from utils import tools


# Version stored and expected in .isolated files.
ISOLATED_FILE_VERSION = '1.4'


# Chunk size to use when doing disk I/O.
DISK_FILE_CHUNK = 1024 * 1024


# Sadly, hashlib uses 'sha1' instead of the standard 'sha-1' so explicitly
# specify the names here.
SUPPORTED_ALGOS = {
  'md5': hashlib.md5,
  'sha-1': hashlib.sha1,
  'sha-512': hashlib.sha512,
}


# Used for serialization.
SUPPORTED_ALGOS_REVERSE = dict((v, k) for k, v in SUPPORTED_ALGOS.iteritems())


class IsolatedError(ValueError):
  """Generic failure to load a .isolated file."""
  pass


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def is_valid_hash(value, algo):
  """Returns if the value is a valid hash for the corresponding algorithm."""
  size = 2 * algo().digest_size
  return bool(re.match(r'^[a-fA-F0-9]{%d}$' % size, value))


def get_hash_algo(_namespace):
  """Return hash algorithm class to use when uploading to given |namespace|."""
  # TODO(vadimsh): Implement this at some point.
  return hashlib.sha1


def is_namespace_with_compression(namespace):
  """Returns True if given |namespace| stores compressed objects."""
  return namespace.endswith(('-gzip', '-deflate'))


def hash_file(filepath, algo):
  """Calculates the hash of a file without reading it all in memory at once.

  |algo| should be one of hashlib hashing algorithm.
  """
  digest = algo()
  with fs.open(filepath, 'rb') as f:
    while True:
      chunk = f.read(DISK_FILE_CHUNK)
      if not chunk:
        break
      digest.update(chunk)
  return digest.hexdigest()


class IsolatedFile(object):
  """Represents a single parsed .isolated file."""

  def __init__(self, obj_hash, algo):
    """|obj_hash| is really the sha-1 of the file."""
    self.obj_hash = obj_hash
    self.algo = algo

    # Raw data.
    self.data = {}
    # A IsolatedFile instance, one per object in self.includes.
    self.children = []

    # Set once the .isolated file is loaded.
    self._is_loaded = False

  def __repr__(self):
    return 'IsolatedFile(%s, loaded: %s)' % (self.obj_hash, self._is_loaded)

  def load(self, content):
    """Verifies the .isolated file is valid and loads this object with the json
    data.
    """
    logging.debug('IsolatedFile.load(%s)' % self.obj_hash)
    assert not self._is_loaded
    self.data = load_isolated(content, self.algo)
    self.children = [
        IsolatedFile(i, self.algo) for i in self.data.get('includes', [])
    ]
    self._is_loaded = True

  @property
  def is_loaded(self):
    """Returns True if 'load' was already called."""
    return self._is_loaded


def walk_includes(isolated):
  """Walks IsolatedFile include graph and yields IsolatedFile objects.

  Visits root node first, then recursively all children, left to right.
  Not yet loaded nodes are considered childless.
  """
  yield isolated
  for child in isolated.children:
    for x in walk_includes(child):
      yield x


@tools.profile
def expand_symlinks(indir, relfile):
  """Follows symlinks in |relfile|, but treating symlinks that point outside the
  build tree as if they were ordinary directories/files. Returns the final
  symlink-free target and a list of paths to symlinks encountered in the
  process.

  The rule about symlinks outside the build tree is for the benefit of the
  Chromium OS ebuild, which symlinks the output directory to an unrelated path
  in the chroot.

  Fails when a directory loop is detected, although in theory we could support
  that case.
  """
  is_directory = relfile.endswith(os.path.sep)
  done = indir
  todo = relfile.strip(os.path.sep)
  symlinks = []

  while todo:
    pre_symlink, symlink, post_symlink = file_path.split_at_symlink(done, todo)
    if not symlink:
      todo = file_path.fix_native_path_case(done, todo)
      done = os.path.join(done, todo)
      break
    symlink_path = os.path.join(done, pre_symlink, symlink)
    post_symlink = post_symlink.lstrip(os.path.sep)
    # readlink doesn't exist on Windows.
    # pylint: disable=E1101
    target = os.path.normpath(os.path.join(done, pre_symlink))
    symlink_target = os.readlink(symlink_path)
    if os.path.isabs(symlink_target):
      # Absolute path are considered a normal directories. The use case is
      # generally someone who puts the output directory on a separate drive.
      target = symlink_target
    else:
      # The symlink itself could be using the wrong path case.
      target = file_path.fix_native_path_case(target, symlink_target)

    if not os.path.exists(target):
      raise MappingError(
          'Symlink target doesn\'t exist: %s -> %s' % (symlink_path, target))
    target = file_path.get_native_path_case(target)
    if not file_path.path_starts_with(indir, target):
      done = symlink_path
      todo = post_symlink
      continue
    if file_path.path_starts_with(target, symlink_path):
      raise MappingError(
          'Can\'t map recursive symlink reference %s -> %s' %
          (symlink_path, target))
    logging.info('Found symlink: %s -> %s', symlink_path, target)
    symlinks.append(os.path.relpath(symlink_path, indir))
    # Treat the common prefix of the old and new paths as done, and start
    # scanning again.
    target = target.split(os.path.sep)
    symlink_path = symlink_path.split(os.path.sep)
    prefix_length = 0
    for target_piece, symlink_path_piece in zip(target, symlink_path):
      if target_piece == symlink_path_piece:
        prefix_length += 1
      else:
        break
    done = os.path.sep.join(target[:prefix_length])
    todo = os.path.join(
        os.path.sep.join(target[prefix_length:]), post_symlink)

  relfile = os.path.relpath(done, indir)
  relfile = relfile.rstrip(os.path.sep) + is_directory * os.path.sep
  return relfile, symlinks


@tools.profile
def expand_directory_and_symlink(indir, relfile, blacklist, follow_symlinks):
  """Expands a single input. It can result in multiple outputs.

  This function is recursive when relfile is a directory.

  Note: this code doesn't properly handle recursive symlink like one created
  with:
    ln -s .. foo
  """
  if os.path.isabs(relfile):
    raise MappingError('Can\'t map absolute path %s' % relfile)

  infile = file_path.normpath(os.path.join(indir, relfile))
  if not infile.startswith(indir):
    raise MappingError('Can\'t map file %s outside %s' % (infile, indir))

  filepath = os.path.join(indir, relfile)
  native_filepath = file_path.get_native_path_case(filepath)
  if filepath != native_filepath:
    # Special case './'.
    if filepath != native_filepath + '.' + os.path.sep:
      # While it'd be nice to enforce path casing on Windows, it's impractical.
      # Also give up enforcing strict path case on OSX. Really, it's that sad.
      # The case where it happens is very specific and hard to reproduce:
      # get_native_path_case(
      #    u'Foo.framework/Versions/A/Resources/Something.nib') will return
      # u'Foo.framework/Versions/A/resources/Something.nib', e.g. lowercase 'r'.
      #
      # Note that this is really something deep in OSX because running
      # ls Foo.framework/Versions/A
      # will print out 'Resources', while file_path.get_native_path_case()
      # returns a lower case 'r'.
      #
      # So *something* is happening under the hood resulting in the command 'ls'
      # and Carbon.File.FSPathMakeRef('path').FSRefMakePath() to disagree.  We
      # have no idea why.
      if sys.platform not in ('darwin', 'win32'):
        raise MappingError(
            'File path doesn\'t equal native file path\n%s != %s' %
            (filepath, native_filepath))

  symlinks = []
  if follow_symlinks:
    try:
      relfile, symlinks = expand_symlinks(indir, relfile)
    except OSError:
      # The file doesn't exist, it will throw below.
      pass

  if relfile.endswith(os.path.sep):
    if not os.path.isdir(infile):
      raise MappingError(
          '%s is not a directory but ends with "%s"' % (infile, os.path.sep))

    # Special case './'.
    if relfile.startswith('.' + os.path.sep):
      relfile = relfile[2:]
    outfiles = symlinks
    try:
      for filename in fs.listdir(infile):
        inner_relfile = os.path.join(relfile, filename)
        if blacklist and blacklist(inner_relfile):
          continue
        if os.path.isdir(os.path.join(indir, inner_relfile)):
          inner_relfile += os.path.sep
        outfiles.extend(
            expand_directory_and_symlink(indir, inner_relfile, blacklist,
                                         follow_symlinks))
      return outfiles
    except OSError as e:
      raise MappingError(
          'Unable to iterate over directory %s.\n%s' % (infile, e))
  else:
    # Always add individual files even if they were blacklisted.
    if os.path.isdir(infile):
      raise MappingError(
          'Input directory %s must have a trailing slash' % infile)

    if not os.path.isfile(infile):
      raise MappingError('Input file %s doesn\'t exist' % infile)

    return symlinks + [relfile]


def expand_directories_and_symlinks(
    indir, infiles, blacklist, follow_symlinks, ignore_broken_items):
  """Expands the directories and the symlinks, applies the blacklist and
  verifies files exist.

  Files are specified in os native path separator.
  """
  outfiles = []
  for relfile in infiles:
    try:
      outfiles.extend(
          expand_directory_and_symlink(
              indir, relfile, blacklist, follow_symlinks))
    except MappingError as e:
      if not ignore_broken_items:
        raise
      logging.info('warning: %s', e)
  return outfiles


@tools.profile
def file_to_metadata(filepath, prevdict, read_only, algo):
  """Processes an input file, a dependency, and return meta data about it.

  Behaviors:
  - Retrieves the file mode, file size, file timestamp, file link
    destination if it is a file link and calcultate the SHA-1 of the file's
    content if the path points to a file and not a symlink.

  Arguments:
    filepath: File to act on.
    prevdict: the previous dictionary. It is used to retrieve the cached sha-1
              to skip recalculating the hash. Optional.
    read_only: If 1 or 2, the file mode is manipulated. In practice, only save
               one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r). On
               windows, mode is not set since all files are 'executable' by
               default.
    algo:      Hashing algorithm used.

  Returns:
    The necessary dict to create a entry in the 'files' section of an .isolated
    file.
  """
  # TODO(maruel): None is not a valid value.
  assert read_only in (None, 0, 1, 2), read_only
  out = {}
  # Always check the file stat and check if it is a link. The timestamp is used
  # to know if the file's content/symlink destination should be looked into.
  # E.g. only reuse from prevdict if the timestamp hasn't changed.
  # There is the risk of the file's timestamp being reset to its last value
  # manually while its content changed. We don't protect against that use case.
  try:
    filestats = os.lstat(filepath)
  except OSError:
    # The file is not present.
    raise MappingError('%s is missing' % filepath)
  is_link = stat.S_ISLNK(filestats.st_mode)

  if sys.platform != 'win32':
    # Ignore file mode on Windows since it's not really useful there.
    filemode = stat.S_IMODE(filestats.st_mode)
    # Remove write access for group and all access to 'others'.
    filemode &= ~(stat.S_IWGRP | stat.S_IRWXO)
    if read_only:
      filemode &= ~stat.S_IWUSR
    if filemode & (stat.S_IXUSR|stat.S_IRGRP) == (stat.S_IXUSR|stat.S_IRGRP):
      # Only keep x group bit if both x user bit and group read bit are set.
      filemode |= stat.S_IXGRP
    else:
      filemode &= ~stat.S_IXGRP
    if not is_link:
      out['m'] = filemode

  # Used to skip recalculating the hash or link destination. Use the most recent
  # update time.
  out['t'] = int(round(filestats.st_mtime))

  if not is_link:
    out['s'] = filestats.st_size
    # If the timestamp wasn't updated and the file size is still the same, carry
    # on the sha-1.
    if (prevdict.get('t') == out['t'] and
        prevdict.get('s') == out['s']):
      # Reuse the previous hash if available.
      out['h'] = prevdict.get('h')
    if not out.get('h'):
      out['h'] = hash_file(filepath, algo)
  else:
    # If the timestamp wasn't updated, carry on the link destination.
    if prevdict.get('t') == out['t']:
      # Reuse the previous link destination if available.
      out['l'] = prevdict.get('l')
    if out.get('l') is None:
      # The link could be in an incorrect path case. In practice, this only
      # happen on OSX on case insensitive HFS.
      # TODO(maruel): It'd be better if it was only done once, in
      # expand_directory_and_symlink(), so it would not be necessary to do again
      # here.
      symlink_value = os.readlink(filepath)  # pylint: disable=E1101
      filedir = file_path.get_native_path_case(os.path.dirname(filepath))
      native_dest = file_path.fix_native_path_case(filedir, symlink_value)
      out['l'] = os.path.relpath(native_dest, filedir)
  return out


def save_isolated(isolated, data):
  """Writes one or multiple .isolated files.

  Note: this reference implementation does not create child .isolated file so it
  always returns an empty list.

  Returns the list of child isolated files that are included by |isolated|.
  """
  # Make sure the data is valid .isolated data by 'reloading' it.
  algo = SUPPORTED_ALGOS[data['algo']]
  load_isolated(json.dumps(data), algo)
  tools.write_json(isolated, data, True)
  return []


def load_isolated(content, algo):
  """Verifies the .isolated file is valid and loads this object with the json
  data.

  Arguments:
  - content: raw serialized content to load.
  - algo: hashlib algorithm class. Used to confirm the algorithm matches the
          algorithm used on the Isolate Server.
  """
  try:
    data = json.loads(content)
  except ValueError:
    raise IsolatedError('Failed to parse: %s...' % content[:100])

  if not isinstance(data, dict):
    raise IsolatedError('Expected dict, got %r' % data)

  # Check 'version' first, since it could modify the parsing after.
  value = data.get('version', '1.0')
  if not isinstance(value, basestring):
    raise IsolatedError('Expected string, got %r' % value)
  try:
    version = tuple(map(int, value.split('.')))
  except ValueError:
    raise IsolatedError('Expected valid version, got %r' % value)

  expected_version = tuple(map(int, ISOLATED_FILE_VERSION.split('.')))
  # Major version must match.
  if version[0] != expected_version[0]:
    raise IsolatedError(
        'Expected compatible \'%s\' version, got %r' %
        (ISOLATED_FILE_VERSION, value))

  if algo is None:
    # TODO(maruel): Remove the default around Jan 2014.
    # Default the algorithm used in the .isolated file itself, falls back to
    # 'sha-1' if unspecified.
    algo = SUPPORTED_ALGOS_REVERSE[data.get('algo', 'sha-1')]

  for key, value in data.iteritems():
    if key == 'algo':
      if not isinstance(value, basestring):
        raise IsolatedError('Expected string, got %r' % value)
      if value not in SUPPORTED_ALGOS:
        raise IsolatedError(
            'Expected one of \'%s\', got %r' %
            (', '.join(sorted(SUPPORTED_ALGOS)), value))
      if value != SUPPORTED_ALGOS_REVERSE[algo]:
        raise IsolatedError(
            'Expected \'%s\', got %r' % (SUPPORTED_ALGOS_REVERSE[algo], value))

    elif key == 'command':
      if not isinstance(value, list):
        raise IsolatedError('Expected list, got %r' % value)
      if not value:
        raise IsolatedError('Expected non-empty command')
      for subvalue in value:
        if not isinstance(subvalue, basestring):
          raise IsolatedError('Expected string, got %r' % subvalue)

    elif key == 'files':
      if not isinstance(value, dict):
        raise IsolatedError('Expected dict, got %r' % value)
      for subkey, subvalue in value.iteritems():
        if not isinstance(subkey, basestring):
          raise IsolatedError('Expected string, got %r' % subkey)
        if not isinstance(subvalue, dict):
          raise IsolatedError('Expected dict, got %r' % subvalue)
        for subsubkey, subsubvalue in subvalue.iteritems():
          if subsubkey == 'l':
            if not isinstance(subsubvalue, basestring):
              raise IsolatedError('Expected string, got %r' % subsubvalue)
          elif subsubkey == 'm':
            if not isinstance(subsubvalue, int):
              raise IsolatedError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'h':
            if not is_valid_hash(subsubvalue, algo):
              raise IsolatedError('Expected sha-1, got %r' % subsubvalue)
          elif subsubkey == 's':
            if not isinstance(subsubvalue, (int, long)):
              raise IsolatedError('Expected int or long, got %r' % subsubvalue)
          else:
            raise IsolatedError('Unknown subsubkey %s' % subsubkey)
        if bool('h' in subvalue) == bool('l' in subvalue):
          raise IsolatedError(
              'Need only one of \'h\' (sha-1) or \'l\' (link), got: %r' %
              subvalue)
        if bool('h' in subvalue) != bool('s' in subvalue):
          raise IsolatedError(
              'Both \'h\' (sha-1) and \'s\' (size) should be set, got: %r' %
              subvalue)
        if bool('s' in subvalue) == bool('l' in subvalue):
          raise IsolatedError(
              'Need only one of \'s\' (size) or \'l\' (link), got: %r' %
              subvalue)
        if bool('l' in subvalue) and bool('m' in subvalue):
          raise IsolatedError(
              'Cannot use \'m\' (mode) and \'l\' (link), got: %r' %
              subvalue)

    elif key == 'includes':
      if not isinstance(value, list):
        raise IsolatedError('Expected list, got %r' % value)
      if not value:
        raise IsolatedError('Expected non-empty includes list')
      for subvalue in value:
        if not is_valid_hash(subvalue, algo):
          raise IsolatedError('Expected sha-1, got %r' % subvalue)

    elif key == 'os':
      if version >= (1, 4):
        raise IsolatedError('Key \'os\' is not allowed starting version 1.4')

    elif key == 'read_only':
      if not value in (0, 1, 2):
        raise IsolatedError('Expected 0, 1 or 2, got %r' % value)

    elif key == 'relative_cwd':
      if not isinstance(value, basestring):
        raise IsolatedError('Expected string, got %r' % value)

    elif key == 'version':
      # Already checked above.
      pass

    else:
      raise IsolatedError('Unknown key %r' % key)

  # Automatically fix os.path.sep if necessary. While .isolated files are always
  # in the the native path format, someone could want to download an .isolated
  # tree from another OS.
  wrong_path_sep = '/' if os.path.sep == '\\' else '\\'
  if 'files' in data:
    data['files'] = dict(
        (k.replace(wrong_path_sep, os.path.sep), v)
        for k, v in data['files'].iteritems())
    for v in data['files'].itervalues():
      if 'l' in v:
        v['l'] = v['l'].replace(wrong_path_sep, os.path.sep)
  if 'relative_cwd' in data:
    data['relative_cwd'] = data['relative_cwd'].replace(
        wrong_path_sep, os.path.sep)
  return data
