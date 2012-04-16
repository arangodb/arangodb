# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module helps emulate Visual Studio 2008 behavior on top of other
build systems, primarily ninja.
"""

import os
import re
import sys

import gyp.MSVSVersion

windows_quoter_regex = re.compile(r'(\\*)"')

def QuoteForRspFile(arg):
  """Quote a command line argument so that it appears as one argument when
  processed via cmd.exe and parsed by CommandLineToArgvW (as is typical for
  Windows programs)."""
  # See http://goo.gl/cuFbX and http://goo.gl/dhPnp including the comment
  # threads. This is actually the quoting rules for CommandLineToArgvW, not
  # for the shell, because the shell doesn't do anything in Windows. This
  # works more or less because most programs (including the compiler, etc.)
  # use that function to handle command line arguments.

  # If the string ends in a \, then it will be interpreted as an escaper for
  # the trailing ", so we need to pre-escape that.
  if arg[-1] == '\\':
    arg = arg + '\\'

  # For a literal quote, CommandLineToArgvW requires 2n+1 backslashes
  # preceding it, and results in n backslashes + the quote. So we substitute
  # in 2* what we match, +1 more, plus the quote.
  arg = windows_quoter_regex.sub(lambda mo: 2 * mo.group(1) + '\\"', arg)

  # %'s also need to be doubled otherwise they're interpreted as batch
  # positional arguments. Also make sure to escape the % so that they're
  # passed literally through escaping so they can be singled to just the
  # original %. Otherwise, trying to pass the literal representation that
  # looks like an environment variable to the shell (e.g. %PATH%) would fail.
  arg = arg.replace('%', '%%')

  # These commands are used in rsp files, so no escaping for the shell (via ^)
  # is necessary.

  # Finally, wrap the whole thing in quotes so that the above quote rule
  # applies and whitespace isn't a word break.
  return '"' + arg + '"'


def EncodeRspFileList(args):
  """Process a list of arguments using QuoteCmdExeArgument."""
  return ' '.join(QuoteForRspFile(arg) for arg in args)


def _GenericRetrieve(root, default, path):
  """Given a list of dictionary keys |path| and a tree of dicts |root|, find
  value at path, or return |default| if any of the path doesn't exist."""
  if not root:
    return default
  if not path:
    return root
  return _GenericRetrieve(root.get(path[0]), default, path[1:])


def _AddPrefix(element, prefix):
  """Add |prefix| to |element| or each subelement if element is iterable."""
  if element is None:
    return element
  # Note, not Iterable because we don't want to handle strings like that.
  if isinstance(element, list) or isinstance(element, tuple):
    return [prefix + e for e in element]
  else:
    return prefix + element


def _DoRemapping(element, map):
  """If |element| then remap it through |map|. If |element| is iterable then
  each item will be remapped. Any elements not found will be removed."""
  if map is not None and element is not None:
    if isinstance(element, list) or isinstance(element, tuple):
      element = filter(None, [map.get(elem) for elem in element])
    else:
      element = map.get(element)
  return element


def _AppendOrReturn(append, element):
  """If |append| is None, simply return |element|. If |append| is not None,
  then add |element| to it, adding each item in |element| if it's a list or
  tuple."""
  if append is not None and element is not None:
    if isinstance(element, list) or isinstance(element, tuple):
      append.extend(element)
    else:
      append.append(element)
  else:
    return element


class MsvsSettings(object):
  """A class that understands the gyp 'msvs_...' values (especially the
  msvs_settings field). They largely correpond to the VS2008 IDE DOM. This
  class helps map those settings to command line options."""

  def __init__(self, spec):
    self.spec = spec
    self.configname = None

    supported_fields = [
        'msvs_configuration_attributes',
        'msvs_settings',
        'msvs_system_include_dirs',
        'msvs_disabled_warnings',
        ]
    configs = spec['configurations']
    for field in supported_fields:
      setattr(self, field, {})
      for configname, config in configs.iteritems():
        getattr(self, field)[configname] = config.get(field, {})

  def _ConvertVSMacros(self, s):
    """Convert from VS macro names to something equivalent."""
    if '$' in s:
      replacements = {
          '$(VSInstallDir)': os.environ.get('VSInstallDir') + '\\',
          '$(VCInstallDir)': os.environ.get('VCInstallDir') + '\\',
          '$(DXSDK_DIR)': os.environ.get('DXSDK_DIR') + '\\',
          '$(OutDir)\\': '',
      }
      for old, new in replacements.iteritems():
        s = s.replace(old, new)
    return s

  def _GetAndMunge(self, field, path, default, prefix, append, map):
    """Retrieve a value from |field| at |path| or return |default|. If
    |append| is specified, and the item is found, it will be appended to that
    object instead of returned. If |map| is specified, results will be
    remapped through |map| before being returned or appended."""
    result = _GenericRetrieve(field, default, path)
    result = _DoRemapping(result, map)
    result = _AddPrefix(result, prefix)
    return _AppendOrReturn(append, result)

  class _GetWrapper(object):
    def __init__(self, parent, field, base_path, append=None):
      self.parent = parent
      self.field = field
      self.base_path = [base_path]
      self.append = append
    def __call__(self, name, map=None, prefix=''):
      return self.parent._GetAndMunge(self.field, self.base_path + [name],
          default=None, prefix=prefix, append=self.append, map=map)

  def Setting(self, path, default=None, prefix='', append=None, map=None):
    """_GetAndMunge for msvs_settings."""
    return self._GetAndMunge(
        self.msvs_settings[self.configname], path, default, prefix, append, map)

  def ConfigAttrib(self, path, default=None, prefix='', append=None, map=None):
    """_GetAndMunge for msvs_configuration_attributes."""
    return self._GetAndMunge(
        self.msvs_configuration_attributes[self.configname],
        path, default, prefix, append, map)

  def GetSystemIncludes(self, config):
    """Returns the extra set of include paths that are used for the Windows
    SDK and similar."""
    return [self._ConvertVSMacros(p)
            for p in self.msvs_system_include_dirs[config]]

  def GetComputedDefines(self, config):
    """Returns the set of defines that are injected to the defines list based
    on other VS settings."""
    self.configname = config
    defines = []
    if self.ConfigAttrib(['CharacterSet']) == '1':
      defines.extend(('_UNICODE', 'UNICODE'))
    if self.ConfigAttrib(['CharacterSet']) == '2':
      defines.append('_MBCS')
    defines.extend(self.Setting(('VCCLCompilerTool', 'PreprocessorDefinitions'),
                                default=[]))
    self.configname = None
    return defines

  def GetCflags(self, config):
    """Returns the flags that need to be added to .c and .cc compilations."""
    cflags = []
    cflags.extend(['/wd' + w for w in self.msvs_disabled_warnings[config]])
    cl = self._GetWrapper(self, self.msvs_settings[config],
                          'VCCLCompilerTool', append=cflags)
    cl('Optimization', map={'0': 'd', '2': 's'}, prefix='/O')
    cl('InlineFunctionExpansion', prefix='/Ob')
    cl('OmitFramePointers', map={'false': '-', 'true': ''}, prefix='/Oy')
    cl('FavorSizeOrSpeed', map={'1': 's', '2': 't'}, prefix='/O')
    cl('WholeProgramOptimization', map={'true': '/GL'})
    cl('WarningLevel', prefix='/W')
    cl('WarnAsError', map={'true': '/WX'})
    cl('DebugInformationFormat',
        map={'1': '7', '3': 'i', '4': 'I'}, prefix='/Z')
    cl('RuntimeTypeInfo', map={'true': '/GR', 'false': '/GR-'})
    cl('EnableFunctionLevelLinking', map={'true': '/Gy', 'false': '/Gy-'})
    cl('MinimalRebuild', map={'true': '/Gm'})
    cl('BufferSecurityCheck', map={'true': '/GS', 'false': '/GS-'})
    cl('BasicRuntimeChecks', map={'1': 's', '2': 'u', '3': '1'}, prefix='/RTC')
    cl('RuntimeLibrary',
        map={'0': 'T', '1': 'Td', '2': 'D', '3': 'Dd'}, prefix='/M')
    cl('ExceptionHandling', map={'1': 'sc','2': 'a'}, prefix='/EH')
    cl('AdditionalOptions', prefix='')
    return cflags

  def GetCflagsC(self, config):
    """Returns the flags that need to be added to .c compilations."""
    return []

  def GetCflagsCC(self, config):
    """Returns the flags that need to be added to .cc compilations."""
    return ['/TP']

  def GetLibFlags(self, config, spec):
    """Returns the flags that need to be added to lib commands."""
    libflags = []
    self.configname = config
    lib = self._GetWrapper(self, self.msvs_settings[config],
                          'VCLibrarianTool', append=libflags)
    libpaths = self.Setting(('VCLibrarianTool', 'AdditionalLibraryDirectories'),
                            default=[])
    libpaths = [os.path.normpath(self._ConvertVSMacros(p)) for p in libpaths]
    libflags.extend(['/LIBPATH:"' + p + '"' for p in libpaths])
    lib('AdditionalOptions')
    self.configname = None
    return libflags

  def GetLdflags(self, config, product_dir, gyp_to_build_path):
    """Returns the flags that need to be added to link commands."""
    ldflags = []
    ld = self._GetWrapper(self, self.msvs_settings[config],
                          'VCLinkerTool', append=ldflags)
    ld('GenerateDebugInformation', map={'true': '/DEBUG'})
    ld('TargetMachine', map={'1': 'X86', '17': 'X64'}, prefix='/MACHINE:')
    ld('AdditionalLibraryDirectories', prefix='/LIBPATH:')
    ld('DelayLoadDLLs', prefix='/DELAYLOAD:')
    ld('AdditionalOptions', prefix='')
    ld('SubSystem', map={'1': 'CONSOLE', '2': 'WINDOWS'}, prefix='/SUBSYSTEM:')
    ld('LinkIncremental', map={'1': ':NO', '2': ''}, prefix='/INCREMENTAL')
    ld('FixedBaseAddress', map={'1': ':NO', '2': ''}, prefix='/FIXED')
    ld('RandomizedBaseAddress',
        map={'1': ':NO', '2': ''}, prefix='/DYNAMICBASE')
    ld('DataExecutionPrevention',
        map={'1': ':NO', '2': ''}, prefix='/NXCOMPAT')
    ld('OptimizeReferences', map={'1': 'NOREF', '2': 'REF'}, prefix='/OPT:')
    ld('EnableCOMDATFolding', map={'1': 'NOICF', '2': 'ICF'}, prefix='/OPT:')
    ld('LinkTimeCodeGeneration', map={'1': '/LTCG'})
    # TODO(scottmg): This should sort of be somewhere else (not really a flag).
    ld('AdditionalDependencies', prefix='')
    # TODO(scottmg): These too.
    ldflags.extend(('kernel32.lib', 'user32.lib', 'gdi32.lib', 'winspool.lib',
        'comdlg32.lib', 'advapi32.lib', 'shell32.lib', 'ole32.lib',
        'oleaut32.lib', 'uuid.lib', 'odbc32.lib', 'DelayImp.lib'))

    # If the base address is not specifically controlled, DYNAMICBASE should
    # be on by default.
    base_flags = filter(lambda x: 'DYNAMICBASE' in x or x == '/FIXED',
                        ldflags)
    if not base_flags:
      ldflags.append('/DYNAMICBASE')

    # If the NXCOMPAT flag has not been specified, default to on. Despite the
    # documentation that says this only defaults to on when the subsystem is
    # Vista or greater (which applies to the linker), the IDE defaults it on
    # unless it's explicitly off.
    if not filter(lambda x: 'NXCOMPAT' in x, ldflags):
      ldflags.append('/NXCOMPAT')

    return ldflags


vs_version = None
def GetVSVersion(generator_flags):
  global vs_version
  if not vs_version:
    vs_version = gyp.MSVSVersion.SelectVisualStudioVersion(
        generator_flags.get('msvs_version', 'auto'))
  return vs_version

def _GetBinaryPath(generator_flags, tool):
  vs = GetVSVersion(generator_flags)
  return ('"' +
          os.path.normpath(os.path.join(vs.Path(), "VC/bin", tool)) +
          '"')

def GetCLPath(generator_flags):
  return _GetBinaryPath(generator_flags, 'cl.exe')

def GetLinkPath(generator_flags):
  return _GetBinaryPath(generator_flags, 'link.exe')

def GetLibPath(generator_flags):
  return _GetBinaryPath(generator_flags, 'lib.exe')

def GetMidlPath(generator_flags):
  return _GetBinaryPath(generator_flags, 'midl.exe')
