#! /usr/bin/python3 -B
# -*- coding: utf-8 -*-
#
# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
# Copyright (C) 2011-2015, International Business Machines
# Corporation and others. All Rights Reserved.
#
# file name: depstest.py
#
# created on: 2011may24

"""ICU dependency tester.

This probably works only on Linux.

The exit code is 0 if everything is fine, 1 for errors, 2 for only warnings.

Sample invocation with an in-source build:
  ~/icu/icu4c/source/test/depstest$ ./depstest.py ../../

Sample invocation with an out-of-source build:
  ~/icu/icu4c/source/test/depstest$ ./depstest.py ~/build/
"""

from __future__ import print_function

__author__ = "Markus W. Scherer"

import glob
import os.path
import subprocess
import sys

import dependencies

_ignored_symbols = set()
_obj_files = {}
_symbols_to_files = {}
_return_value = 0

# Classes with vtables (and thus virtual methods).
_virtual_classes = set()
# Classes with weakly defined destructors.
# nm shows a symbol class of "W" rather than "T".
_weak_destructors = set()

def iteritems(items):
  """Python 2/3-compatible iteritems"""
  try:
    for v in items.iteritems():
      yield v
  except AttributeError:
    for v in items.items():
      yield v

def _ReadObjFile(root_path, library_name, obj_name):
  global _ignored_symbols, _obj_files, _symbols_to_files
  global _virtual_classes, _weak_destructors
  lib_obj_name = library_name + "/" + obj_name
  if lib_obj_name in _obj_files:
    print("Warning: duplicate .o file " + lib_obj_name)
    _return_value = 2
    return

  path = os.path.join(root_path, library_name, obj_name)
  nm_result = subprocess.Popen(["nm", "--demangle", "--format=sysv",
                                "--extern-only", "--no-sort", path],
                               stdout=subprocess.PIPE).communicate()[0]
  obj_imports = set()
  obj_exports = set()
  for line in nm_result.splitlines():
    fields = line.decode().split("|")
    if len(fields) == 1: continue
    name = fields[0].strip()
    # Ignore symbols like '__cxa_pure_virtual',
    # 'vtable for __cxxabiv1::__si_class_type_info' or
    # 'DW.ref.__gxx_personality_v0'.
    # '__dso_handle' belongs to __cxa_atexit().
    if (name.startswith("__cxa") or "__cxxabi" in name or "__gxx" in name or
        name == "__dso_handle"):
      _ignored_symbols.add(name)
      continue
    type = fields[2].strip()
    if type == "U":
      obj_imports.add(name)
    else:
      obj_exports.add(name)
      _symbols_to_files[name] = lib_obj_name
      # Is this a vtable? E.g., "vtable for icu_49::ByteSink".
      if name.startswith("vtable for icu"):
        _virtual_classes.add(name[name.index("::") + 2:])
      # Is this a destructor? E.g., "icu_49::ByteSink::~ByteSink()".
      index = name.find("::~")
      if index >= 0 and type == "W":
        _weak_destructors.add(name[index + 3:name.index("(", index)])
  _obj_files[lib_obj_name] = {"imports": obj_imports, "exports": obj_exports}

def _ReadLibrary(root_path, library_name):
  obj_paths = glob.glob(os.path.join(root_path, library_name, "*.o"))
  for path in obj_paths:
    _ReadObjFile(root_path, library_name, os.path.basename(path))

# Dependencies that would otherwise be errors, but that are to be allowed
# in a limited (not transitive) context.  List of (file_name, symbol)
# TODO: Move this data to dependencies.txt?
allowed_errors = (
  ("common/umutex.o", "std::__throw_system_error(int)"),
  ("common/umutex.o", "std::uncaught_exception()"),
  ("common/umutex.o", "std::__once_callable"),
  ("common/umutex.o", "std::__once_call"),
  ("common/umutex.o", "__once_proxy"),
  ("common/umutex.o", "__tls_get_addr"),
  ("common/unifiedcache.o", "std::__throw_system_error(int)"),
  # Some of the MessageFormat 2 modules reference exception-related symbols
  # in instantiations of the `std::get()` method that gets an alternative
  # from a `std::variant`.
  # These instantiations of `std::get()` are only called by compiler-generated
  # code (the implementations of built-in `swap()` methods for types
  # that include a `std::variant`; and `std::__detail::__variant::__gen_vtable_impl()`,
  # which constructs vtables. The MessageFormat 2 code itself only calls
  # `std::get_if()`, which is exception-free; never `std::get()`.
  ("i18n/messageformat2_data_model.o", "typeinfo for std::exception"),
  ("i18n/messageformat2_data_model.o", "vtable for std::exception"),
  ("i18n/messageformat2_data_model.o", "std::exception::~exception()"),
  ("i18n/messageformat2_formattable.o", "typeinfo for std::exception"),
  ("i18n/messageformat2_formattable.o", "vtable for std::exception"),
  ("i18n/messageformat2_formattable.o", "std::exception::~exception()"),
  ("i18n/messageformat2_function_registry.o", "typeinfo for std::exception"),
  ("i18n/messageformat2_function_registry.o", "vtable for std::exception"),
  ("i18n/messageformat2_function_registry.o", "std::exception::~exception()")
)

def _Resolve(name, parents):
  global _ignored_symbols, _obj_files, _symbols_to_files, _return_value
  item = dependencies.items[name]
  item_type = item["type"]
  if name in parents:
    sys.exit("Error: %s %s has a circular dependency on itself: %s" %
             (item_type, name, parents))
  # Check if already cached.
  exports = item.get("exports")
  if exports != None: return item
  # Calculate recursively.
  parents.append(name)
  imports = set()
  exports = set()
  system_symbols = item.get("system_symbols")
  if system_symbols == None: system_symbols = item["system_symbols"] = set()
  files = item.get("files")
  if files:
    for file_name in files:
      obj_file = _obj_files[file_name]
      imports |= obj_file["imports"]
      exports |= obj_file["exports"]
  imports -= exports | _ignored_symbols
  deps = item.get("deps")
  if deps:
    for dep in deps:
      dep_item = _Resolve(dep, parents)
      # Detect whether this item needs to depend on dep,
      # except when this item has no files, that is, when it is just
      # a deliberate umbrella group or library.
      dep_exports = dep_item["exports"]
      dep_system_symbols = dep_item["system_symbols"]
      if files and imports.isdisjoint(dep_exports) and imports.isdisjoint(dep_system_symbols):
        print("Info:  %s %s  does not need to depend on  %s\n" % (item_type, name, dep))
      # We always include the dependency's exports, even if we do not need them
      # to satisfy local imports.
      exports |= dep_exports
      system_symbols |= dep_system_symbols
  item["exports"] = exports
  item["system_symbols"] = system_symbols
  imports -= exports | system_symbols
  for symbol in imports:
    for file_name in files:
      if (file_name, symbol) in allowed_errors:
         sys.stderr.write("Info:  ignoring %s imports %s\n\n" % (file_name, symbol))
         continue
      if symbol in _obj_files[file_name]["imports"]:
        neededFile = _symbols_to_files.get(symbol)
        if neededFile in dependencies.file_to_item:
          neededItem = "but %s does not depend on %s (for %s)" % (name, dependencies.file_to_item[neededFile], neededFile)
        else:
          neededItem = "- is this a new system symbol?"
        sys.stderr.write("Error: in %s %s: %s imports %s %s\n" %
                         (item_type, name, file_name, symbol, neededItem))
        _return_value = 1
  del parents[-1]
  return item

def Process(root_path):
  """Loads dependencies.txt, reads the libraries' .o files, and processes them.

  Modifies dependencies.items: Recursively builds each item's system_symbols and exports.
  """
  global _ignored_symbols, _obj_files, _return_value
  global _virtual_classes, _weak_destructors
  dependencies.Load()
  for name_and_item in iteritems(dependencies.items):
    name = name_and_item[0]
    item = name_and_item[1]
    system_symbols = item.get("system_symbols")
    if system_symbols:
      for symbol in system_symbols:
        _symbols_to_files[symbol] = name
  for library_name in dependencies.libraries:
    _ReadLibrary(root_path, library_name)
  o_files_set = set(_obj_files.keys())
  files_missing_from_deps = o_files_set - dependencies.files
  files_missing_from_build = dependencies.files - o_files_set
  if files_missing_from_deps:
    sys.stderr.write("Error: files missing from dependencies.txt:\n%s\n" %
                     sorted(files_missing_from_deps))
    _return_value = 1
  if files_missing_from_build:
    sys.stderr.write("Error: files in dependencies.txt but not built:\n%s\n" %
                     sorted(files_missing_from_build))
    _return_value = 1
  if not _return_value:
    for library_name in dependencies.libraries:
      _Resolve(library_name, [])
  if not _return_value:
    virtual_classes_with_weak_destructors = _virtual_classes & _weak_destructors
    if virtual_classes_with_weak_destructors:
      sys.stderr.write("Error: Some classes have virtual methods, and "
                       "an implicit or inline destructor "
                       "(see ICU ticket #8454 for details):\n%s\n" %
                       sorted(virtual_classes_with_weak_destructors))
      _return_value = 1

def main():
  global _return_value
  if len(sys.argv) <= 1:
    sys.exit(("Command line error: " +
             "need one argument with the root path to the built ICU libraries/*.o files."))
  Process(sys.argv[1])
  if _ignored_symbols:
    print("Info: ignored symbols:\n%s" % sorted(_ignored_symbols))
  if not _return_value:
    print("OK: Specified and actual dependencies match.")
  else:
    print("Error: There were errors, please fix them and re-run. Processing may have terminated abnormally.")
  return _return_value

if __name__ == "__main__":
  sys.exit(main())
