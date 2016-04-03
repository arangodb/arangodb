# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Contains logic to parse .isolate files.

This module doesn't touch the file system. It's the job of the client code to do
I/O on behalf of this module.

See more information at
  https://code.google.com/p/swarming/wiki/IsolateDesign
  https://code.google.com/p/swarming/wiki/IsolateUserGuide
"""

import ast
import itertools
import logging
import os
import posixpath
import re
import sys

from utils import fs


# Valid variable name.
VALID_VARIABLE = '[A-Za-z_][A-Za-z_0-9]*'


class IsolateError(ValueError):
  """Generic failure to load a .isolate file."""
  pass


def determine_root_dir(relative_root, infiles):
  """For a list of infiles, determines the deepest root directory that is
  referenced indirectly.

  All arguments must be using os.path.sep.
  """
  # The trick used to determine the root directory is to look at "how far" back
  # up it is looking up.
  deepest_root = relative_root
  for i in infiles:
    x = relative_root
    while i.startswith('..' + os.path.sep):
      i = i[3:]
      assert not i.startswith(os.path.sep)
      x = os.path.dirname(x)
    if deepest_root.startswith(x):
      deepest_root = x
  logging.info(
      'determine_root_dir(%s, %d files) -> %s',
      relative_root, len(infiles), deepest_root)
  return deepest_root


def replace_variable(part, variables):
  m = re.match(r'<\((' + VALID_VARIABLE + ')\)', part)
  if m:
    if m.group(1) not in variables:
      raise IsolateError(
        'Variable "%s" was not found in %s.\nDid you forget to specify '
        '--path-variable?' % (m.group(1), variables))
    return str(variables[m.group(1)])
  return part


def eval_variables(item, variables):
  """Replaces the .isolate variables in a string item.

  Note that the .isolate format is a subset of the .gyp dialect.
  """
  return ''.join(
      replace_variable(p, variables)
      for p in re.split(r'(<\(' + VALID_VARIABLE + '\))', item))


def pretty_print(variables, stdout):
  """Outputs a .isolate file from the decoded variables.

  The .isolate format is GYP compatible.

  Similar to pprint.print() but with NIH syndrome.
  """
  # Order the dictionary keys by these keys in priority.
  ORDER = ('variables', 'condition', 'command', 'files', 'read_only')

  def sorting_key(x):
    """Gives priority to 'most important' keys before the others."""
    if x in ORDER:
      return str(ORDER.index(x))
    return x

  def loop_list(indent, items):
    for item in items:
      if isinstance(item, basestring):
        stdout.write('%s\'%s\',\n' % (indent, item))
      elif isinstance(item, dict):
        stdout.write('%s{\n' % indent)
        loop_dict(indent + '  ', item)
        stdout.write('%s},\n' % indent)
      elif isinstance(item, list):
        # A list inside a list will write the first item embedded.
        stdout.write('%s[' % indent)
        for index, i in enumerate(item):
          if isinstance(i, basestring):
            stdout.write(
                '\'%s\', ' % i.replace('\\', '\\\\').replace('\'', '\\\''))
          elif isinstance(i, dict):
            stdout.write('{\n')
            loop_dict(indent + '  ', i)
            if index != len(item) - 1:
              x = ', '
            else:
              x = ''
            stdout.write('%s}%s' % (indent, x))
          else:
            assert False
        stdout.write('],\n')
      else:
        assert False

  def loop_dict(indent, items):
    for key in sorted(items, key=sorting_key):
      item = items[key]
      stdout.write("%s'%s': " % (indent, key))
      if isinstance(item, dict):
        stdout.write('{\n')
        loop_dict(indent + '  ', item)
        stdout.write(indent + '},\n')
      elif isinstance(item, list):
        stdout.write('[\n')
        loop_list(indent + '  ', item)
        stdout.write(indent + '],\n')
      elif isinstance(item, basestring):
        stdout.write(
            '\'%s\',\n' % item.replace('\\', '\\\\').replace('\'', '\\\''))
      elif isinstance(item, (int, bool)) or item is None:
        stdout.write('%s,\n' % item)
      else:
        assert False, item

  stdout.write('{\n')
  loop_dict('  ', variables)
  stdout.write('}\n')


def print_all(comment, data, stream):
  """Prints a complete .isolate file and its top-level file comment into a
  stream.
  """
  if comment:
    stream.write(comment)
  pretty_print(data, stream)


def extract_comment(content):
  """Extracts file level comment."""
  out = []
  for line in content.splitlines(True):
    if line.startswith('#'):
      out.append(line)
    else:
      break
  return ''.join(out)


def eval_content(content):
  """Evaluates a python file and return the value defined in it.

  Used in practice for .isolate files.
  """
  globs = {'__builtins__': None}
  locs = {}
  try:
    value = eval(content, globs, locs)
  except TypeError as e:
    e.args = list(e.args) + [content]
    raise
  assert locs == {}, locs
  assert globs == {'__builtins__': None}, globs
  return value


def match_configs(expr, config_variables, all_configs):
  """Returns the list of values from |values| that match the condition |expr|.

  Arguments:
    expr: string that is evaluatable with eval(). It is a GYP condition.
    config_variables: list of the name of the variables.
    all_configs: list of the list of possible values.

  If a variable is not referenced at all, it is marked as unbounded (free) with
  a value set to None.
  """
  # It is more than just eval'ing the variable, it needs to be double checked to
  # see if the variable is referenced at all. If not, the variable is free
  # (unbounded).
  # TODO(maruel): Use the intelligent way by inspecting expr instead of doing
  # trial and error to figure out which variable is bound.
  combinations = []
  for bound_variables in itertools.product(
      (True, False), repeat=len(config_variables)):
    # Add the combination of variables bound.
    combinations.append(
        (
          [c for c, b in zip(config_variables, bound_variables) if b],
          set(
            tuple(v if b else None for v, b in zip(line, bound_variables))
            for line in all_configs)
        ))

  out = []
  for variables, configs in combinations:
    # Strip variables and see if expr can still be evaluated.
    for values in configs:
      globs = {'__builtins__': None}
      globs.update(zip(variables, (v for v in values if v is not None)))
      try:
        assertion = eval(expr, globs, {})
      except NameError:
        continue
      if not isinstance(assertion, bool):
        raise IsolateError('Invalid condition')
      if assertion:
        out.append(values)
  return out


def verify_variables(variables):
  """Verifies the |variables| dictionary is in the expected format."""
  VALID_VARIABLES = [
    'command',
    'files',
    'read_only',
  ]
  assert isinstance(variables, dict), variables
  assert set(VALID_VARIABLES).issuperset(set(variables)), variables.keys()
  for name, value in variables.iteritems():
    if name == 'read_only':
      assert value in (0, 1, 2, None), value
    else:
      assert isinstance(value, list), value
      assert all(isinstance(i, basestring) for i in value), value


def verify_ast(expr, variables_and_values):
  """Verifies that |expr| is of the form
  expr ::= expr ( "or" | "and" ) expr
         | identifier "==" ( string | int )
  Also collects the variable identifiers and string/int values in the dict
  |variables_and_values|, in the form {'var': set([val1, val2, ...]), ...}.
  """
  assert isinstance(expr, (ast.BoolOp, ast.Compare))
  if isinstance(expr, ast.BoolOp):
    assert isinstance(expr.op, (ast.And, ast.Or))
    for subexpr in expr.values:
      verify_ast(subexpr, variables_and_values)
  else:
    assert isinstance(expr.left.ctx, ast.Load)
    assert len(expr.ops) == 1
    assert isinstance(expr.ops[0], ast.Eq)
    var_values = variables_and_values.setdefault(expr.left.id, set())
    rhs = expr.comparators[0]
    assert isinstance(rhs, (ast.Str, ast.Num))
    var_values.add(rhs.n if isinstance(rhs, ast.Num) else rhs.s)


def verify_condition(condition, variables_and_values):
  """Verifies the |condition| dictionary is in the expected format.
  See verify_ast() for the meaning of |variables_and_values|.
  """
  VALID_INSIDE_CONDITION = ['variables']
  assert isinstance(condition, list), condition
  assert len(condition) == 2, condition
  expr, then = condition

  test_ast = compile(expr, '<condition>', 'eval', ast.PyCF_ONLY_AST)
  verify_ast(test_ast.body, variables_and_values)

  assert isinstance(then, dict), then
  assert set(VALID_INSIDE_CONDITION).issuperset(set(then)), then.keys()
  if not 'variables' in then:
    raise IsolateError('Missing \'variables\' in condition %s' % condition)
  verify_variables(then['variables'])


def verify_root(value, variables_and_values):
  """Verifies that |value| is the parsed form of a valid .isolate file.

  See verify_ast() for the meaning of |variables_and_values|.
  """
  VALID_ROOTS = ['includes', 'conditions', 'variables']
  assert isinstance(value, dict), value
  assert set(VALID_ROOTS).issuperset(set(value)), value.keys()

  includes = value.get('includes', [])
  assert isinstance(includes, list), includes
  for include in includes:
    assert isinstance(include, basestring), include

  conditions = value.get('conditions', [])
  assert isinstance(conditions, list), conditions
  for condition in conditions:
    verify_condition(condition, variables_and_values)

  variables = value.get('variables', {})
  verify_variables(variables)


def get_folders(values_dict):
  """Returns a dict of all the folders in the given value_dict."""
  return dict(
    (item, configs) for (item, configs) in values_dict.iteritems()
    if item.endswith('/')
  )


class ConfigSettings(object):
  """Represents the dependency variables for a single build configuration.

  The structure is immutable.

  .command and .isolate_dir describe how to run the command. .isolate_dir uses
      the OS' native path separator. It must be an absolute path, it's the path
      where to start the command from.
  .files is the list of dependencies. The items use '/' as a path separator.
  .read_only describe how to map the files.
  """
  def __init__(self, values, isolate_dir):
    verify_variables(values)
    if isolate_dir is None:
      # It must be an empty object if isolate_dir is None.
      assert values == {}, values
    else:
      # Otherwise, the path must be absolute.
      assert os.path.isabs(isolate_dir), isolate_dir

    self.files = sorted(values.get('files', []))
    self.command = values.get('command', [])[:]
    self.isolate_dir = isolate_dir
    self.read_only = values.get('read_only')

  def union(self, rhs):
    """Merges two config settings together into a new instance.

    A new instance is not created and self or rhs is returned if the other
    object is the empty object.

    self has priority over rhs for .command. Use the same .isolate_dir as the
    one having a .command.

    Dependencies listed in rhs are patch adjusted ONLY if they don't start with
    a path variable, e.g. the characters '<('.
    """
    # When an object has .isolate_dir == None, it means it is the empty object.
    if rhs.isolate_dir is None:
      return self
    if self.isolate_dir is None:
      return rhs

    if sys.platform == 'win32':
      assert self.isolate_dir[0].lower() == rhs.isolate_dir[0].lower()

    # Takes the difference between the two isolate_dir. Note that while
    # isolate_dir is in native path case, all other references are in posix.
    l_rel_cwd, r_rel_cwd = self.isolate_dir, rhs.isolate_dir
    if self.command or rhs.command:
      use_rhs = bool(not self.command and rhs.command)
    else:
      # If self doesn't define any file, use rhs.
      use_rhs = not bool(self.files)
    if use_rhs:
      # Rebase files in rhs.
      l_rel_cwd, r_rel_cwd = r_rel_cwd, l_rel_cwd

    rebase_path = os.path.relpath(r_rel_cwd, l_rel_cwd).replace(
        os.path.sep, '/')
    def rebase_item(f):
      if f.startswith('<(') or rebase_path == '.':
        return f
      return posixpath.join(rebase_path, f)

    def map_both(l, r):
      """Rebase items in either lhs or rhs, as needed."""
      if use_rhs:
        l, r = r, l
      return sorted(l + map(rebase_item, r))

    var = {
      'command': self.command or rhs.command,
      'files': map_both(self.files, rhs.files),
      'read_only': rhs.read_only if self.read_only is None else self.read_only,
    }
    return ConfigSettings(var, l_rel_cwd)

  def flatten(self):
    """Converts the object into a dict."""
    out = {}
    if self.command:
      out['command'] = self.command
    if self.files:
      out['files'] = self.files
    if self.read_only is not None:
      out['read_only'] = self.read_only
    # TODO(maruel): Probably better to not output it if command is None?
    if self.isolate_dir is not None:
      out['isolate_dir'] = self.isolate_dir
    return out

  def __str__(self):
    """Returns a short representation useful for debugging."""
    files = ''.join('\n    ' + f for f in self.files)
    return 'ConfigSettings(%s, %s, %s, %s)' % (
        self.command,
        self.isolate_dir,
        self.read_only,
        files or '[]')


def _safe_index(l, k):
  try:
    return l.index(k)
  except ValueError:
    return None


def _get_map_keys(dest_keys, in_keys):
  """Returns a tuple of the indexes of each item in in_keys found in dest_keys.

  For example, if in_keys is ('A', 'C') and dest_keys is ('A', 'B', 'C'), the
  return value will be (0, None, 1).
  """
  return tuple(_safe_index(in_keys, k) for k in dest_keys)


def _map_keys(mapping, items):
  """Returns a tuple with items placed at mapping index.

  For example, if mapping is (1, None, 0) and items is ('a', 'b'), it will
  return ('b', None, 'c').
  """
  return tuple(items[i] if i != None else None for i in mapping)


class Configs(object):
  """Represents a processed .isolate file.

  Stores the file in a processed way, split by configuration.

  At this point, we don't know all the possibilities. So mount a partial view
  that we have.

  This class doesn't hold isolate_dir, since it is dependent on the final
  configuration selected. It is implicitly dependent on which .isolate defines
  the 'command' that will take effect.
  """
  def __init__(self, file_comment, config_variables):
    self.file_comment = file_comment
    # Contains the names of the config variables seen while processing
    # .isolate file(s). The order is important since the same order is used for
    # keys in self._by_config.
    assert isinstance(config_variables, tuple)
    assert all(isinstance(c, basestring) for c in config_variables), (
        config_variables)
    config_variables = tuple(config_variables)
    assert tuple(sorted(config_variables)) == config_variables, config_variables
    self._config_variables = config_variables
    # The keys of _by_config are tuples of values for each of the items in
    # self._config_variables. A None item in the list of the key means the value
    # is unbounded.
    self._by_config = {}

  @property
  def config_variables(self):
    return self._config_variables

  def get_config(self, config):
    """Returns all configs that matches this config as a single ConfigSettings.

    Returns an empty ConfigSettings if none apply.
    """
    # TODO(maruel): Fix ordering based on the bounded values. The keys are not
    # necessarily sorted in the way that makes sense, they are alphabetically
    # sorted. It is important because the left-most takes predescence.
    out = ConfigSettings({}, None)
    for k, v in sorted(self._by_config.iteritems()):
      if all(i == j or j is None for i, j in zip(config, k)):
        out = out.union(v)
    return out

  def set_config(self, key, value):
    """Sets the ConfigSettings for this key.

    The key is a tuple of bounded or unbounded variables. The global variable
    is the key where all values are unbounded, e.g.:
      (None,) * len(self._config_variables)
    """
    assert key not in self._by_config, (key, self._by_config.keys())
    assert isinstance(key, tuple)
    assert len(key) == len(self._config_variables), (
        key, self._config_variables)
    assert isinstance(value, ConfigSettings)
    self._by_config[key] = value

  def union(self, rhs):
    """Returns a new Configs instance, the union of variables from self and rhs.

    Uses self.file_comment if available, otherwise rhs.file_comment.
    It keeps config_variables sorted in the output.
    """
    # Merge the keys of config_variables for each Configs instances. All the new
    # variables will become unbounded. This requires realigning the keys.
    config_variables = tuple(sorted(
        set(self.config_variables) | set(rhs.config_variables)))
    out = Configs(self.file_comment or rhs.file_comment, config_variables)
    mapping_lhs = _get_map_keys(out.config_variables, self.config_variables)
    mapping_rhs = _get_map_keys(out.config_variables, rhs.config_variables)
    lhs_config = dict(
        (_map_keys(mapping_lhs, k), v) for k, v in self._by_config.iteritems())
    # pylint: disable=W0212
    rhs_config = dict(
        (_map_keys(mapping_rhs, k), v) for k, v in rhs._by_config.iteritems())

    for key in set(lhs_config) | set(rhs_config):
      l = lhs_config.get(key)
      r = rhs_config.get(key)
      out.set_config(key, l.union(r) if (l and r) else (l or r))
    return out

  def flatten(self):
    """Returns a flat dictionary representation of the configuration.
    """
    return dict((k, v.flatten()) for k, v in self._by_config.iteritems())

  def __str__(self):
    return 'Configs(%s,%s)' % (
      self._config_variables,
      ''.join('\n  %s' % str(f) for f in self._by_config))


def load_included_isolate(isolate_dir, isolate_path):
  if os.path.isabs(isolate_path):
    raise IsolateError(
        'Failed to load configuration; absolute include path \'%s\'' %
        isolate_path)
  included_isolate = os.path.normpath(os.path.join(isolate_dir, isolate_path))
  if sys.platform == 'win32':
    if included_isolate[0].lower() != isolate_dir[0].lower():
      raise IsolateError(
          'Can\'t reference a .isolate file from another drive')
  with fs.open(included_isolate, 'r') as f:
    return load_isolate_as_config(
        os.path.dirname(included_isolate),
        eval_content(f.read()),
        None)


def load_isolate_as_config(isolate_dir, value, file_comment):
  """Parses one .isolate file and returns a Configs() instance.

  Arguments:
    isolate_dir: only used to load relative includes so it doesn't depend on
                 cwd.
    value: is the loaded dictionary that was defined in the gyp file.
    file_comment: comments found at the top of the file so it can be preserved.

  The expected format is strict, anything diverting from the format below will
  throw an assert:
  {
    'includes': [
      'foo.isolate',
    ],
    'conditions': [
      ['OS=="vms" and foo=42', {
        'variables': {
          'command': [
            ...
          ],
          'files': [
            ...
          ],
          'read_only': 0,
        },
      }],
      ...
    ],
    'variables': {
      ...
    },
  }
  """
  assert os.path.isabs(isolate_dir), isolate_dir
  if any(len(cond) == 3 for cond in value.get('conditions', [])):
    raise IsolateError('Using \'else\' is not supported anymore.')
  variables_and_values = {}
  verify_root(value, variables_and_values)
  if variables_and_values:
    config_variables, config_values = zip(
        *sorted(variables_and_values.iteritems()))
    all_configs = list(itertools.product(*config_values))
  else:
    config_variables = ()
    all_configs = []

  isolate = Configs(file_comment, config_variables)

  # Add global variables. The global variables are on the empty tuple key.
  isolate.set_config(
      (None,) * len(config_variables),
      ConfigSettings(value.get('variables', {}), isolate_dir))

  # Add configuration-specific variables.
  for expr, then in value.get('conditions', []):
    configs = match_configs(expr, config_variables, all_configs)
    new = Configs(None, config_variables)
    for config in configs:
      new.set_config(config, ConfigSettings(then['variables'], isolate_dir))
    isolate = isolate.union(new)

  # If the .isolate contains command, ignore any command in child .isolate.
  root_has_command = any(c.command for c in isolate._by_config.itervalues())

  # Load the includes. Process them in reverse so the last one take precedence.
  for include in reversed(value.get('includes', [])):
    included = load_included_isolate(isolate_dir, include)
    if root_has_command:
      # Strip any command in the imported isolate. It is because the chosen
      # command is not related to the one in the top-most .isolate, since the
      # configuration is flattened.
      for c in included._by_config.itervalues():
        c.command = []
    isolate = isolate.union(included)

  return isolate


def load_isolate_for_config(isolate_dir, content, config_variables):
  """Loads the .isolate file and returns the information unprocessed but
  filtered for the specific OS.

  Returns:
    tuple of command, dependencies, read_only flag, isolate_dir.
    The dependencies are fixed to use os.path.sep.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  isolate = load_isolate_as_config(isolate_dir, eval_content(content), None)
  try:
    config_name = tuple(
        config_variables[var] for var in isolate.config_variables)
  except KeyError:
    raise IsolateError(
        'These configuration variables were missing from the command line: %s' %
        ', '.join(
            sorted(set(isolate.config_variables) - set(config_variables))))

  # A configuration is to be created with all the combinations of free
  # variables.
  config = isolate.get_config(config_name)
  dependencies = [f.replace('/', os.path.sep) for f in config.files]
  return config.command, dependencies, config.read_only, config.isolate_dir
