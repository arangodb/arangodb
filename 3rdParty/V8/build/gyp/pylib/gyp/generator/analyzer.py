# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script is intended for use as a GYP_GENERATOR. It takes as input (by way of
the generator flag config_path) the path of a json file that dictates the files
and targets to search for. The following keys are supported:
files: list of paths (relative) of the files to search for.
targets: list of targets to search for. The target names are unqualified.

The following is output:
error: only supplied if there is an error.
warning: only supplied if there is a warning.
targets: the set of targets passed in via targets that either directly or
  indirectly depend upon the set of paths supplied in files.
status: indicates if any of the supplied files matched at least one target.

If the generator flag analyzer_output_path is specified, output is written
there. Otherwise output is written to stdout.
"""

import gyp.common
import gyp.ninja_syntax as ninja_syntax
import json
import os
import posixpath
import sys

debug = False

found_dependency_string = 'Found dependency'
no_dependency_string = 'No dependencies'

# MatchStatus is used indicate if and how a target depends upon the supplied
# sources.
# The target's sources contain one of the supplied paths.
MATCH_STATUS_MATCHES = 1
# The target has a dependency on another target that contains one of the
# supplied paths.
MATCH_STATUS_MATCHES_BY_DEPENDENCY = 2
# The target's sources weren't in the supplied paths and none of the target's
# dependencies depend upon a target that matched.
MATCH_STATUS_DOESNT_MATCH = 3
# The target doesn't contain the source, but the dependent targets have not yet
# been visited to determine a more specific status yet.
MATCH_STATUS_TBD = 4

generator_supports_multiple_toolsets = True

generator_wants_static_library_dependencies_adjusted = False

generator_default_variables = {
}
for dirname in ['INTERMEDIATE_DIR', 'SHARED_INTERMEDIATE_DIR', 'PRODUCT_DIR',
                'LIB_DIR', 'SHARED_LIB_DIR']:
  generator_default_variables[dirname] = '!!!'

for unused in ['RULE_INPUT_PATH', 'RULE_INPUT_ROOT', 'RULE_INPUT_NAME',
               'RULE_INPUT_DIRNAME', 'RULE_INPUT_EXT',
               'EXECUTABLE_PREFIX', 'EXECUTABLE_SUFFIX',
               'STATIC_LIB_PREFIX', 'STATIC_LIB_SUFFIX',
               'SHARED_LIB_PREFIX', 'SHARED_LIB_SUFFIX',
               'CONFIGURATION_NAME']:
  generator_default_variables[unused] = ''

def __ExtractBasePath(target):
  """Extracts the path components of the specified gyp target path."""
  last_index = target.rfind('/')
  if last_index == -1:
    return ''
  return target[0:(last_index + 1)]

def __ResolveParent(path, base_path_components):
  """Resolves |path|, which starts with at least one '../'. Returns an empty
  string if the path shouldn't be considered. See __AddSources() for a
  description of |base_path_components|."""
  depth = 0
  while path.startswith('../'):
    depth += 1
    path = path[3:]
  # Relative includes may go outside the source tree. For example, an action may
  # have inputs in /usr/include, which are not in the source tree.
  if depth > len(base_path_components):
    return ''
  if depth == len(base_path_components):
    return path
  return '/'.join(base_path_components[0:len(base_path_components) - depth]) + \
      '/' + path

def __AddSources(sources, base_path, base_path_components, result):
  """Extracts valid sources from |sources| and adds them to |result|. Each
  source file is relative to |base_path|, but may contain '..'. To make
  resolving '..' easier |base_path_components| contains each of the
  directories in |base_path|. Additionally each source may contain variables.
  Such sources are ignored as it is assumed dependencies on them are expressed
  and tracked in some other means."""
  # NOTE: gyp paths are always posix style.
  for source in sources:
    if not len(source) or source.startswith('!!!') or source.startswith('$'):
      continue
    # variable expansion may lead to //.
    org_source = source
    source = source[0] + source[1:].replace('//', '/')
    if source.startswith('../'):
      source = __ResolveParent(source, base_path_components)
      if len(source):
        result.append(source)
      continue
    result.append(base_path + source)
    if debug:
      print 'AddSource', org_source, result[len(result) - 1]

def __ExtractSourcesFromAction(action, base_path, base_path_components,
                               results):
  if 'inputs' in action:
    __AddSources(action['inputs'], base_path, base_path_components, results)

def __ExtractSources(target, target_dict, toplevel_dir):
  # |target| is either absolute or relative and in the format of the OS. Gyp
  # source paths are always posix. Convert |target| to a posix path relative to
  # |toplevel_dir_|. This is done to make it easy to build source paths.
  if os.sep == '\\' and os.altsep == '/':
    base_path = target.replace('\\', '/')
  else:
    base_path = target
  if base_path == toplevel_dir:
    base_path = ''
  elif base_path.startswith(toplevel_dir + '/'):
    base_path = base_path[len(toplevel_dir) + len('/'):]
  base_path = posixpath.dirname(base_path)
  base_path_components = base_path.split('/')

  # Add a trailing '/' so that __AddSources() can easily build paths.
  if len(base_path):
    base_path += '/'

  if debug:
    print 'ExtractSources', target, base_path

  results = []
  if 'sources' in target_dict:
    __AddSources(target_dict['sources'], base_path, base_path_components,
                 results)
  # Include the inputs from any actions. Any changes to these effect the
  # resulting output.
  if 'actions' in target_dict:
    for action in target_dict['actions']:
      __ExtractSourcesFromAction(action, base_path, base_path_components,
                                 results)
  if 'rules' in target_dict:
    for rule in target_dict['rules']:
      __ExtractSourcesFromAction(rule, base_path, base_path_components, results)

  return results

class Target(object):
  """Holds information about a particular target:
  deps: set of the names of direct dependent targets.
  match_staus: one of the MatchStatus values"""
  def __init__(self):
    self.deps = set()
    self.match_status = MATCH_STATUS_TBD

class Config(object):
  """Details what we're looking for
  look_for_dependency_only: if true only search for a target listing any of
                            the files in files.
  files: set of files to search for
  targets: see file description for details"""
  def __init__(self):
    self.look_for_dependency_only = True
    self.files = []
    self.targets = []

  def Init(self, params):
    """Initializes Config. This is a separate method as it may raise an
    exception if there is a parse error."""
    generator_flags = params.get('generator_flags', {})
    # TODO(sky): nuke file_path and look_for_dependency_only once migrate
    # recipes.
    file_path = generator_flags.get('file_path', None)
    if file_path:
      self._InitFromFilePath(file_path)
      return

    # If |file_path| wasn't specified then we look for config_path.
    # TODO(sky): always look for config_path once migrated recipes.
    config_path = generator_flags.get('config_path', None)
    if not config_path:
      return
    self.look_for_dependency_only = False
    try:
      f = open(config_path, 'r')
      config = json.load(f)
      f.close()
    except IOError:
      raise Exception('Unable to open file ' + config_path)
    except ValueError as e:
      raise Exception('Unable to parse config file ' + config_path + str(e))
    if not isinstance(config, dict):
      raise Exception('config_path must be a JSON file containing a dictionary')
    self.files = config.get('files', [])
    # Coalesce duplicates
    self.targets = list(set(config.get('targets', [])))

  def _InitFromFilePath(self, file_path):
    try:
      f = open(file_path, 'r')
      for file_name in f:
        if file_name.endswith('\n'):
          file_name = file_name[0:len(file_name) - 1]
          if len(file_name):
            self.files.append(file_name)
      f.close()
    except IOError:
      raise Exception('Unable to open file', file_path)

def __GenerateTargets(target_list, target_dicts, toplevel_dir, files):
  """Generates a dictionary with the key the name of a target and the value a
  Target. |toplevel_dir| is the root of the source tree. If the sources of
  a target match that of |files|, then |target.matched| is set to True.
  This returns a tuple of the dictionary and whether at least one target's
  sources listed one of the paths in |files|."""
  targets = {}

  # Queue of targets to visit.
  targets_to_visit = target_list[:]

  matched = False

  while len(targets_to_visit) > 0:
    target_name = targets_to_visit.pop()
    if target_name in targets:
      continue

    target = Target()
    targets[target_name] = target
    sources = __ExtractSources(target_name, target_dicts[target_name],
                               toplevel_dir)
    for source in sources:
      if source in files:
        target.match_status = MATCH_STATUS_MATCHES
        matched = True
        break

    for dep in target_dicts[target_name].get('dependencies', []):
      targets[target_name].deps.add(dep)
      targets_to_visit.append(dep)

  return targets, matched

def _GetUnqualifiedToQualifiedMapping(all_targets, to_find):
  """Returns a mapping (dictionary) from unqualified name to qualified name for
  all the targets in |to_find|."""
  result = {}
  if not to_find:
    return result
  to_find = set(to_find)
  for target_name in all_targets.keys():
    extracted = gyp.common.ParseQualifiedTarget(target_name)
    if len(extracted) > 1 and extracted[1] in to_find:
      to_find.remove(extracted[1])
      result[extracted[1]] = target_name
      if not to_find:
        return result
  return result

def _DoesTargetDependOn(target, all_targets):
  """Returns true if |target| or any of its dependencies matches the supplied
  set of paths. This updates |matches| of the Targets as it recurses.
  target: the Target to look for.
  all_targets: mapping from target name to Target.
  matching_targets: set of targets looking for."""
  if target.match_status == MATCH_STATUS_DOESNT_MATCH:
    return False
  if target.match_status == MATCH_STATUS_MATCHES or \
      target.match_status == MATCH_STATUS_MATCHES_BY_DEPENDENCY:
    return True
  for dep_name in target.deps:
    dep_target = all_targets[dep_name]
    if _DoesTargetDependOn(dep_target, all_targets):
      dep_target.match_status = MATCH_STATUS_MATCHES_BY_DEPENDENCY
      return True
    dep_target.match_status = MATCH_STATUS_DOESNT_MATCH
  return False

def _GetTargetsDependingOn(all_targets, possible_targets):
  """Returns the list of targets in |possible_targets| that depend (either
  directly on indirectly) on the matched files.
  all_targets: mapping from target name to Target.
  possible_targets: targets to search from."""
  found = []
  for target in possible_targets:
    if _DoesTargetDependOn(all_targets[target], all_targets):
      # possible_targets was initially unqualified, keep it unqualified.
      found.append(gyp.common.ParseQualifiedTarget(target)[1])
  return found

def _WriteOutput(params, **values):
  """Writes the output, either to stdout or a file is specified."""
  output_path = params.get('generator_flags', {}).get(
      'analyzer_output_path', None)
  if not output_path:
    print json.dumps(values)
    return
  try:
    f = open(output_path, 'w')
    f.write(json.dumps(values) + '\n')
    f.close()
  except IOError as e:
    print 'Error writing to output file', output_path, str(e)

def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  flavor = gyp.common.GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
  elif flavor == 'win':
    default_variables.setdefault('OS', 'win')
    # Copy additional generator configuration data from VS, which is shared
    # by the Windows Ninja generator.
    import gyp.generator.msvs as msvs_generator
    generator_additional_non_configuration_keys = getattr(msvs_generator,
        'generator_additional_non_configuration_keys', [])
    generator_additional_path_sections = getattr(msvs_generator,
        'generator_additional_path_sections', [])

    gyp.msvs_emulation.CalculateCommonVariables(default_variables, params)
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)

def GenerateOutput(target_list, target_dicts, data, params):
  """Called by gyp as the final stage. Outputs results."""
  config = Config()
  try:
    config.Init(params)
    if not config.files:
      if config.look_for_dependency_only:
        print 'Must specify files to analyze via file_path generator flag'
        return
      raise Exception('Must specify files to analyze via config_path generator '
                      'flag')

    toplevel_dir = os.path.abspath(params['options'].toplevel_dir)
    if os.sep == '\\' and os.altsep == '/':
      toplevel_dir = toplevel_dir.replace('\\', '/')
    if debug:
      print 'toplevel_dir', toplevel_dir

    all_targets, matched = __GenerateTargets(target_list, target_dicts,
                                             toplevel_dir,
                                             frozenset(config.files))

    # Set of targets that refer to one of the files.
    if config.look_for_dependency_only:
      print found_dependency_string if matched else no_dependency_string
      return

    warning = None
    if matched:
      unqualified_mapping = _GetUnqualifiedToQualifiedMapping(
          all_targets, config.targets)
      if len(unqualified_mapping) != len(config.targets):
        not_found = []
        for target in config.targets:
          if not target in unqualified_mapping:
            not_found.append(target)
        warning = 'Unable to find all targets: ' + str(not_found)
      qualified_targets = []
      for target in config.targets:
        if target in unqualified_mapping:
          qualified_targets.append(unqualified_mapping[target])
      output_targets = _GetTargetsDependingOn(all_targets, qualified_targets)
    else:
      output_targets = []

    result_dict = { 'targets': output_targets,
                    'status': found_dependency_string if matched else
                              no_dependency_string }
    if warning:
      result_dict['warning'] = warning
    _WriteOutput(params, **result_dict)

  except Exception as e:
    _WriteOutput(params, error=str(e))
