#! /usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2011, International Business Machines
# Corporation and others. All Rights Reserved.
#
# file name: dependencies.py
#
# created on: 2011may26

"""Reader module for dependency data for the ICU dependency tester.

Reads dependencies.txt and makes the data available.

Attributes:
  files: Set of "library/filename.o" files mentioned in the dependencies file.
  items: Map from library or group names to item maps.
    Each item has a "type" ("library" or "group" or "system_symbols").
    A library or group item can have an optional set of "files" (as in the files attribute).
    Each item can have an optional set of "deps" (libraries & groups).
    A group item also has a "library" name unless it is a group of system symbols.
    The one "system_symbols" item and its groups have sets of "system_symbols"
    with standard-library system symbol names.
  libraries: Set of library names mentioned in the dependencies file.
"""
__author__ = "Markus W. Scherer"

# TODO: Support binary items.
# .txt syntax:   binary: tools/genrb
# item contents: {"type": "binary"} with optional files & deps
# A binary must not be used as a dependency for anything else.

import sys

files = set()
items = {}
libraries = set()

_line_number = 0
_groups_to_be_defined = set()

def _CheckLibraryName(name):
  global _line_number
  if not name:
    sys.exit("Error:%d: \"library: \" without name" % _line_number)
  if name.endswith(".o"):
    sys.exit("Error:%d: invalid library name %s"  % (_line_number, name))

def _CheckGroupName(name):
  global _line_number
  if not name:
    sys.exit("Error:%d: \"group: \" without name" % _line_number)
  if "/" in name or name.endswith(".o"):
    sys.exit("Error:%d: invalid group name %s"  % (_line_number, name))

def _CheckFileName(name):
  global _line_number
  if "/" in name or not name.endswith(".o"):
    sys.exit("Error:%d: invalid file name %s"  % (_line_number, name))

def _RemoveComment(line):
  global _line_number
  _line_number = _line_number + 1
  index = line.find("#")  # Remove trailing comment.
  if index >= 0: line = line[:index]
  return line.rstrip()  # Remove trailing newlines etc.

def _ReadLine(f):
  while True:
    line = _RemoveComment(f.next())
    if line: return line

def _ReadFiles(deps_file, item, library_name):
  global files
  item_files = item.get("files")
  while True:
    line = _ReadLine(deps_file)
    if not line: continue
    if not line.startswith("    "): return line
    if item_files == None: item_files = item["files"] = set()
    for file_name in line.split():
      _CheckFileName(file_name)
      file_name = library_name + "/" + file_name
      if file_name in files:
        sys.exit("Error:%d: file %s listed in multiple groups" % (_line_number, file_name))
      files.add(file_name)
      item_files.add(file_name)

def _IsLibrary(item): return item and item["type"] == "library"

def _IsLibraryGroup(item): return item and "library" in item

def _ReadDeps(deps_file, item, library_name):
  global items, _line_number, _groups_to_be_defined
  item_deps = item.get("deps")
  while True:
    line = _ReadLine(deps_file)
    if not line: continue
    if not line.startswith("    "): return line
    if item_deps == None: item_deps = item["deps"] = set()
    for dep in line.split():
      _CheckGroupName(dep)
      dep_item = items.get(dep)
      if item["type"] == "system_symbols" and (_IsLibraryGroup(dep_item) or _IsLibrary(dep_item)):
        sys.exit(("Error:%d: system_symbols depend on previously defined " +
                  "library or library group %s") % (_line_number, dep))
      if dep_item == None:
        # Add this dependency as a new group.
        items[dep] = {"type": "group"}
        if library_name: items[dep]["library"] = library_name
        _groups_to_be_defined.add(dep)
      item_deps.add(dep)

def _AddSystemSymbol(item, symbol):
  exports = item.get("system_symbols")
  if exports == None: exports = item["system_symbols"] = set()
  exports.add(symbol)

def _ReadSystemSymbols(deps_file, item):
  global _line_number
  while True:
    line = _ReadLine(deps_file)
    if not line: continue
    if not line.startswith("    "): return line
    line = line.lstrip()
    if '"' in line:
      # One double-quote-enclosed symbol on the line, allows spaces in a symbol name.
      symbol = line[1:-1]
      if line.startswith('"') and line.endswith('"') and '"' not in symbol:
        _AddSystemSymbol(item, symbol)
      else:
        sys.exit("Error:%d: invalid quoted symbol name %s" % (_line_number, line))
    else:
      # One or more space-separate symbols.
      for symbol in line.split(): _AddSystemSymbol(item, symbol)

def Load():
  """Reads "dependencies.txt" and populates the module attributes."""
  global items, libraries, _line_number, _groups_to_be_defined
  deps_file = open("dependencies.txt")
  try:
    line = None
    current_type = None
    while True:
      while not line: line = _RemoveComment(deps_file.next())

      if line.startswith("library: "):
        current_type = "library"
        name = line[9:].lstrip()
        _CheckLibraryName(name)
        if name in items:
          sys.exit("Error:%d: library definition using duplicate name %s" % (_line_number, name))
        libraries.add(name)
        item = items[name] = {"type": "library"}
        line = _ReadFiles(deps_file, item, name)
      elif line.startswith("group: "):
        current_type = "group"
        name = line[7:].lstrip()
        _CheckGroupName(name)
        if name not in items:
          sys.exit("Error:%d: group %s defined before mentioned as a dependency" %
                   (_line_number, name))
        if name not in _groups_to_be_defined:
          sys.exit("Error:%d: group definition using duplicate name %s" % (_line_number, name))
        _groups_to_be_defined.remove(name)
        item = items[name]
        library_name = item.get("library")
        if library_name:
          line = _ReadFiles(deps_file, item, library_name)
        else:
          line = _ReadSystemSymbols(deps_file, item)
      elif line == "  deps":
        if current_type == "library":
          line = _ReadDeps(deps_file, items[name], name)
        elif current_type == "group":
          item = items[name]
          line = _ReadDeps(deps_file, item, item.get("library"))
        elif current_type == "system_symbols":
          item = items[current_type]
          line = _ReadDeps(deps_file, item, None)
        else:
          sys.exit("Error:%d: deps before any library or group" % _line_number)
      elif line == "system_symbols:":
        current_type = "system_symbols"
        if current_type in items:
          sys.exit("Error:%d: duplicate entry for system_symbols" % _line_number)
        item = items[current_type] = {"type": current_type}
        line = _ReadSystemSymbols(deps_file, item)
      else:
        sys.exit("Syntax error:%d: %s" % (_line_number, line))
  except StopIteration:
    pass
  if _groups_to_be_defined:
    sys.exit("Error: some groups mentioned in dependencies are undefined: %s" % _groups_to_be_defined)
