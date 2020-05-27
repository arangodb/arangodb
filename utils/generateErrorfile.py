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


# generate C implementation file from errors
def genCFile(errors, filename):

  headerfile = os.path.splitext(filename)[0] + ".h"

  impl = prologue\
         + "#include \"Basics/Common.h\"\n"\
         + "#include \"Basics/error.h\"\n"\
         + "#include \"Basics/voc-errors.h\"\n"\
         + "\n"\
         + "/// helper macro to define an error string\n"\
         + "#define REG_ERROR(id, label) TRI_set_errno_string(TRI_ ## id, label);\n"\
         + "\n"\
         + "void TRI_InitializeErrorMessages() {\n"

  # print individual errors
  for e in errors:
    msg  = e[2].replace("\n", " ").replace("\\", "").replace("\"", "\\\"")
    impl = impl\
           + "  REG_ERROR(" + e[0] + ", \"" + msg + "\");\n"

  impl = impl\
       + "}\n"

  return impl


# generate C header file from errors
def genCHeaderFile(errors):
  wiki = "/// Error codes and meanings\n"\
       + "/// The following errors might be raised when running ArangoDB:\n"\
       + "\n\n"

  header =   "#ifndef ARANGODB_BASICS_VOC_ERRORS_H\n"\
           + "#define ARANGODB_BASICS_VOC_ERRORS_H 1\n"\
           + "\n"\
           + wiki

  # print individual errors
  for e in errors:
    header = header\
           + "/// " + e[1] + ": " + e[0] + "\n"\
           + wrap(e[2], 80, 0, 0, "/// \"") + "\"\n"\
           + wrap(e[3], 80, 0, 0, "/// ") + "\n"\
           + "constexpr int TRI_" + e[0].ljust(61) + " = " + e[1] + ";\n"\
           + "\n"

  header = header + "\n"\
           + "/// register all errors for ArangoDB\n"\
           + "void TRI_InitializeErrorMessages();\n"\
           + "\n"

  header = header\
         + "#endif\n"

  return header


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
filename = outfile

if extension == ".tmp":
  filename = os.path.splitext(outfile)[0]
  extension = os.path.splitext(filename)[1]

if extension == ".js":
  out = genJsFile(errorsList)
elif extension == ".h":
  out = genCHeaderFile(errorsList)
elif extension == ".cpp":
  out = genCFile(errorsList, filename)
else:
  print("usage: {} <sourcefile> <outfile>".format(sys.argv[0]), file=sys.stderr)
  sys.exit(1)

with io.open(outfile, mode="w", encoding="utf-8", newline="") as out_fh:
    out_fh.write(out)
