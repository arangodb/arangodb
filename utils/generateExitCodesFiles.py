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
      + "  internal.exitCodes = {\n"

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
      + "}());\n"\
      + "\n"

  return out

# generate NSIS implementation file from errors
def genNSISFile(errors, filename):

  impl = """
!include "LogicLib.nsh"
"""
  for e in errors:
    impl += """!define ARANGO_%s %s
""" % (
  e[0],
  e[1]
  )

  impl +="""
!macro printExitCode exitCode Message DetailMessage
  Push "${exitCode}"
  Push "${Message}"
  Push "${DetailMessage}"
  Call printExitCode
!macroend
Function printExitCode
pop $3
pop $2
pop $1
${Switch} $1\n
"""
  # print individual errors
  for e in errors:
    impl += """
  ${Case} %s # %s
    MessageBox MB_ICONEXCLAMATION '$2:$\\r$\\n>> %s <<$\\r$\\n"%s"$\\r$\\n$3'
  ${Break}
""" % (
  e[1],
  e[0],
  e[2],
  e[3]
  )

  impl = impl + """
  ${Default}
    MessageBox MB_ICONEXCLAMATION '$2:$\\r$\\nUnknown exit code $1!'
    ; Will be returned if the recovery fails
  ${Break}

${EndSwitch}
FunctionEnd
"""

  return impl.replace("\r", "\r\n")


# generate C header file from errors
def genCHeaderFile(errors):
  wiki = "/// Exit codes and meanings\n"\
       + "/// The following codes might be returned when exiting ArangoDB:\n"

  header =   "#ifndef ARANGODB_BASICS_EXIT_CODES_H\n"\
           + "#define ARANGODB_BASICS_EXIT_CODES_H 1\n"\
           + "\n"\
           + "#include \"Basics/error.h\"\n"\
           + "\n"\
           + wiki\
           + "\n"

  # print individual errors
  for e in errors:
    header = header\
           + "/// " + e[1] + ": " + e[0] + "\n"\
           + wrap(e[2], 80, 0, 0, "/// ") + "\n"\
           + wrap(e[3], 80, 0, 0, "/// ") + "\n"\
           + "constexpr int TRI_" + e[0].ljust(61) + " = " + e[1] + ";\n"\
           + "\n"

  header = header\
         + "\n"\
         + "#endif\n"\
         + "\n"

  return header


# define some globals
prologue = "/// auto-generated file generated from exitcodes.dat\n"\
         + "\n"

if len(sys.argv) < 3:
  print("usage: {} <sourcefile> <outfile>".format(sys.argv[0]), file=sys.stderr)
  sys.exit(1)

source = sys.argv[1]

# read input file
errors=None
errorsList = []
with io.open(source, encoding="utf-8", newline=None) as source_fh:
    errors = csv.reader(source_fh)

    r1 = re.compile(r'^#.*')

    for e in errors:
      if len(e) == 0:
        continue

      if r1.match(e[0]):
        continue

      if e[0] == "" or e[1] == "" or e[2] == "" or e[3] == "":
        print("invalid exit code declaration file: {}".format(source), file=sys.stderr)
        sys.exit(1)

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
elif extension == ".nsh":
  out = genNSISFile(errorsList, filename)
else:
  print("usage: {} <sourcefile> <outfile>".format(sys.argv[0]), file=sys.stderr)
  sys.exit(1)

with io.open(outfile, mode="w", encoding="utf-8", newline="") as out_fh:
    out_fh.write(out)
