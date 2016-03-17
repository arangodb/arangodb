import csv, sys, os.path, re

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


# generate javascript file from mimetypes
def genJsFile(types):
  jslint = "/*jslint indent: 2,\n"\
           "         nomen: true,\n"\
           "         maxlen: 100,\n"\
           "         sloppy: true,\n"\
           "         vars: true,\n"\
           "         white: true,\n"\
           "         plusplus: true */\n"\
           "/*global exports */\n\n"

  out = jslint \
      + prologue\
      + "exports.mimeTypes = {\n"
  
  extensions = { }
  # print individual mimetypes
  i = 0
  for t in types:
    extension = t[0]
    mimetype = t[1]
    out = out + "  \"" + extension + "\": [ \"" + mimetype + "\", " + t[2] + " ]"

    if not mimetype in extensions:
      extensions[mimetype] = [ ]

    extensions[mimetype].append(extension)
    i = i + 1 

    if i < len(types):
      out = out + ", \n"
    else:
      out = out + "\n"

  out = out + "};\n\n"

  # print extensions
  out = out + "exports.extensions = {\n"
  i = 0
  for e in extensions:

    out = out + "  \"" + e + "\": [ \"" + "\", \"".join(extensions[e]) + "\" ]"
    i = i + 1 

    if i < len(extensions):
      out = out + ", \n"
    else:
      out = out + "\n"
      
  out = out + "};\n\n"

  return out


# generate C header file from errors
def genCHeaderFile(types):
  header = "\n"\
           + "#ifndef LIB_BASICS_VOC_MIMETYPES_H\n"\
           + "#define LIB_BASICS_VOC_MIMETYPES_H 1\n"\
           + "\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "/// @brief initialize mimetypes\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "\n"\
           + "void TRI_InitializeEntriesMimetypes();\n"\
           + "\n"\
           + "#endif\n"

  return header


# generate C implementation file from mimetypes
def genCFile(types, filename):

  headerfile = os.path.splitext(filename)[0] + ".h"

  impl = prologue\
         + "#include \"Basics/Common.h\"\n\n"\
         + "#include \"Basics/mimetypes.h\"\n"\
         + "#include \"" + headerfile + "\"\n"\
         + "\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @brief initialize mimetypes\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"\
         + "void TRI_InitializeEntriesMimetypes() {\n"

  # print individual types
  for t in types:
    impl = impl + "  TRI_RegisterMimetype(\"" + t[0] + "\", \"" + t[1] + "\", " + t[2] + ");\n"

  impl = impl\
       + "}\n"

  return impl


# define some globals 
prologue = "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// AUTO-GENERATED FILE GENERATED FROM mimetypes.dat\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// DISCLAIMER\n"\
         + "///\n"\
         + "/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany\n"\
         + "/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany\n"\
         + "///\n"\
         + "/// Licensed under the Apache License, Version 2.0 (the \"License\");\n"\
         + "/// you may not use this file except in compliance with the License.\n"\
         + "/// You may obtain a copy of the License at\n"\
         + "///\n"\
         + "///     http://www.apache.org/licenses/LICENSE-2.0\n"\
         + "///\n"\
         + "/// Unless required by applicable law or agreed to in writing, software\n"\
         + "/// distributed under the License is distributed on an \"AS IS\" BASIS,\n"\
         + "/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"\
         + "/// See the License for the specific language governing permissions and\n"\
         + "/// limitations under the License.\n"\
         + "///\n"\
         + "/// Copyright holder is ArangoDB GmbH, Cologne, Germany\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
 
if len(sys.argv) < 3:
  print >> sys.stderr, "usage: %s <sourcefile> <outfile>" % sys.argv[0]
  sys.exit()

source = sys.argv[1]

# read input file
mimetypes = csv.reader(open(source, "rb"))
types = []

r1 = re.compile(r'^#.*')

for t in mimetypes:
  if len(t) == 0:
    continue

  if r1.match(t[0]):
    continue

  t[2] = t[2].strip()
  if t[0] == "" or t[1] == "" or not (t[2] == "true" or t[2] == "false"):
    print >> sys.stderr, "invalid mimetypes declaration file: %s" % (source)
    sys.exit()

  types.append(t)

outfile = sys.argv[2]
extension = os.path.splitext(outfile)[1]
filename = outfile

if extension == ".tmp":
  filename = os.path.splitext(outfile)[0]
  extension = os.path.splitext(filename)[1]

if extension == ".js":
  out = genJsFile(types)
elif extension == ".h":
  out = genCHeaderFile(types)
elif extension == ".cpp":
  out = genCFile(types, filename)
else:
  print >> sys.stderr, "usage: %s <sourcefile> <outfile>" % sys.argv[0]
  sys.exit()

outFile = open(outfile, "wb")
outFile.write(out);
outFile.close()

