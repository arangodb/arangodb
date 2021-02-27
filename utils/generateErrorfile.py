#!/usr/bin/env python3
from __future__ import print_function
import csv, sys, os.path, re, io

# wrap text after x characters
def wrap(string, width=80, ind1=0, ind2=0, prefix=''):
  string = prefix + ind1 * " " + string
  newstring = ""
  string = string.replace("\n", " ")

  while len(string) > width:
    marker = width - 1
    while not string[marker].isspace():
      marker = marker - 1

    newline = string[0:marker] + "\n"
    newstring = newstring + newline
    string = prefix + ind2 * " " + string[marker + 1:]

  return newstring + string


# generate javascript file from errors
def genJsFile(errors):
  jslint = "/*jshint maxlen: 240 */\n/*global require */\n\n"

  out = jslint \
      + prologue\
      + "(function () {\n"\
      + "  \"use strict\";\n"\
      + "  var internal = require(\"internal\");\n"\
      + "\n"\
      + "  internal.errors = {\n"

  # print individual errors
  i = 0
  for e in errors:
    name = "\"" + e[0] + "\""
    msg  = e[2].replace("\n", " ").replace("\\", "").replace("\"", "\\\"")
    out = out\
        + "    " + name.ljust(30) + " : { \"code\" : " + e[1] + ", \"message\" : \"" + msg + "\" }"

    i = i + 1

    if i < len(errors):
      out = out + ",\n"
    else:
      out = out + "\n"


  out = out\
      + "  };\n"\
      + "\n"\
      + "  // For compatibility with <= 3.3\n"\
      + "  internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND = internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;\n"\
      + "}());\n"\
      + "\n"

  return out

def quotedErrorMessage(error):
    msg  = error[2].replace("\n", " ").replace("\\", "").replace("\"", "\\\"")
    return "\"" + msg + "\""

def errorName(error):
    return "TRI_" + error[0]

# generate C header file from errors
def genCHeaderFile(errors):
  wiki = "/// Error codes and meanings\n"\
       + "/// The following errors might be raised when running ArangoDB:\n"\
       + "\n\n"

  header = """
#ifndef ARANGODB_BASICS_VOC_ERRORS_H
#define ARANGODB_BASICS_VOC_ERRORS_H 1

#include "Basics/ErrorCode.h"

""" \
           + wiki

  # print individual errors
  for e in errors:
    header = header\
           + "/// " + e[1] + ": " + e[0] + "\n"\
           + wrap(e[2], 80, 0, 0, "/// \"") + "\"\n"\
           + wrap(e[3], 80, 0, 0, "/// ") + "\n"\
           + "constexpr auto " + errorName(e).ljust(65) + " = ErrorCode{" + e[1] + "};\n"\
           + "\n"

  header = header\
         + "#endif\n"

  return header

def genErrorRegistryHeaderFile(errors):
    template = """
#ifndef ARANGODB_BASICS_ERROR_REGISTRY_H
#define ARANGODB_BASICS_ERROR_REGISTRY_H

#include "Basics/voc-errors.h"

namespace frozen {{
template <>
struct elsa<ErrorCode> {{
  constexpr auto operator()(ErrorCode const& value, std::size_t seed) const -> std::size_t {{
    return elsa<int>{{}}.operator()(static_cast<int>(value), seed);
  }}
}};
}}  // namespace frozen

#include <frozen/unordered_map.h>

namespace arangodb::error {{
constexpr static frozen::unordered_map<ErrorCode, const char*, {numErrorMessages}> ErrorMessages = {{
{initializerList}
}};
}}

#endif  // ARANGODB_BASICS_ERROR_REGISTRY_H
"""
    initializerList = '\n'.join([
        "    {" + errorName(e) + ",  // " + e[1] + "\n" + \
        "      " + quotedErrorMessage(e) + "},"
        for e in errors
    ])

    return template.format(numErrorMessages = len(errors), initializerList = initializerList)


# define some globals
prologue = "/// auto-generated file generated from errors.dat\n"\
         + "\n"

if len(sys.argv) < 3:
  print("usage: {} <sourcefile> <outfile>".format(sys.argv[0]), file=sys.stderr)
  sys.exit(1)

source = sys.argv[1]

# read input file

erros = []
with io.open(source, encoding="utf-8", newline=None) as source_fh:
    errors = csv.reader(io.open(source, encoding="utf-8", newline=None))

    errorsList = []

    r1 = re.compile(r'^#.*')

    for e in errors:
      if len(e) == 0:
        continue

      if r1.match(e[0]):
        continue

      if e[0] == "" or e[1] == "" or e[2] == "" or e[3] == "":
        print("invalid error declaration file: {}".format(source), file=sys.stderr)
        sys.exit()

      errorsList.append(e)

outfile = sys.argv[2]
extension = os.path.splitext(outfile)[1]

basename = os.path.basename(outfile)
filename = basename if extension != ".tmp" else os.path.splitext(basename)[0]

if filename == "errors.js":
  out = genJsFile(errorsList)
elif filename == "voc-errors.h":
  out = genCHeaderFile(errorsList)
elif filename == "error-registry.h":
  out = genErrorRegistryHeaderFile(errorsList)
else:
  print("usage: {} <sourcefile> <outfile>".format(sys.argv[0]), file=sys.stderr)
  sys.exit(1)

with io.open(outfile, mode="w", encoding="utf-8", newline="") as out_fh:
    out_fh.write(out)
