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
           + "#ifndef TRIAGENS_BASICS_C_VOC_MIMETYPES_H\n"\
           + "#define TRIAGENS_BASICS_C_VOC_MIMETYPES_H 1\n"\
           + "\n"\
           + "#ifdef __cplusplus\n"\
           + "extern \"C\" {\n"\
           + "#endif\n"\
           + "\n"\
           + docstart\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "/// @brief initialise mimetypes\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "\n"\
           + "void TRI_InitialiseEntriesMimetypes (void);\n"\
           + docend\
           + "#ifdef __cplusplus\n"\
           + "}\n"\
           + "#endif\n"\
           + "\n"\
           + "#endif\n"\
           + "\n"

  return header


# generate C implementation file from mimetypes
def genCFile(types, filename):

  headerfile = os.path.splitext(filename)[0] + ".h"

  impl = prologue\
         + "#include <BasicsC/common.h>\n"\
         + "#include \"" + headerfile + "\"\n"\
         + "\n"\
         + docstart\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @brief initialise mimetypes\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"\
         + "void TRI_InitialiseEntriesMimetypes (void) {\n"

  # print individual types
  for t in types:
    impl = impl + "  TRI_RegisterMimetype(\"" + t[0] + "\", \"" + t[1] + "\", " + t[2] + ");\n"

  impl = impl\
       + "}\n"\
       + docend

  return impl


# define some globals 
prologue = "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @brief auto-generated file generated from mimetypes.dat\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"
  
docstart = "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @addtogroup Mimetypes\n"\
         + "/// @{\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"
  
docend   = "\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @}\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"



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

  if t[0] == "" or t[1] == "" or not (t[2] == "true" or t[2] == "false"):
    print >> sys.stderr, "invalid mimetypes declaration file: %s (line %i)" % (source, i)
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
elif extension == ".c":
  out = genCFile(types, filename)
else:
  print >> sys.stderr, "usage: %s <sourcefile> <outfile>" % sys.argv[0]
  sys.exit()

outFile = open(outfile, "wb")
outFile.write(out);
outFile.close()

