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


# generate javascript file from errors
def genJsFile(errors):
  jslint = "/*jslint indent: 2,\n"\
           "         nomen: true,\n"\
           "         maxlen: 240,\n"\
           "         sloppy: true,\n"\
           "         vars: true,\n"\
           "         white: true,\n"\
           "         plusplus: true */\n"\
           "/*global require */\n\n"

  out = jslint \
      + prologue\
      + "(function () {\n"\
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
      out = out + ", \n"
    else:
      out = out + "\n"


  out = out\
      + "  };\n"\
      + "}());\n"\
      + "\n"

  return out


# generate C implementation file from errors
def genCFile(errors, filename):

  headerfile = os.path.splitext(filename)[0] + ".h"

  impl = prologue\
         + "#include <BasicsC/common.h>\n"\
         + "#include \"" + headerfile + "\"\n"\
         + "\n"\
         + docstart\
         + "void TRI_InitialiseErrorMessages (void) {\n"

  # print individual errors
  for e in errors:
    msg  = e[2].replace("\n", " ").replace("\\", "").replace("\"", "\\\"")
    impl = impl\
           + "  REG_ERROR(" + e[0] + ", \"" + msg + "\");\n"

  impl = impl\
       + "}\n"\
       + docend

  return impl


# generate C header file from errors
def genCHeaderFile(errors):
  wiki = "////////////////////////////////////////////////////////////////////////////////\n"\
       + "/// @page ArangoErrors Error codes and meanings\n"\
       + "///\n"\
       + "/// The following errors might be raised when running ArangoDB:\n"\
       + "///\n"
  
  for e in errors:
    wiki = wiki\
         + "/// - " + e[1] + ": @LIT{" + e[2].replace("%", "\%").replace("<", "\<").replace(">", "\>") + "}\n"\
         + wrap(e[3], 80, 0, 0, "///   ") + "\n"

  wiki = wiki\
       + "////////////////////////////////////////////////////////////////////////////////\n"

  header = "\n"\
           + "#ifndef TRIAGENS_BASICS_C_VOC_ERRORS_H\n"\
           + "#define TRIAGENS_BASICS_C_VOC_ERRORS_H 1\n"\
           + "\n"\
           + "#ifdef __cplusplus\n"\
           + "extern \"C\" {\n"\
           + "#endif\n"\
           + "\n"\
           + wiki\
           + "\n"\
           + docstart\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "/// @brief helper macro to define an error string\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "\n"\
           + "#define REG_ERROR(id, label) TRI_set_errno_string(TRI_ ## id, label);\n"\
           + "\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "/// @brief register all errors for ArangoDB\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "\n"\
           + "void TRI_InitialiseErrorMessages (void);\n"\
           + "\n"
 
  # print individual errors
  for e in errors:
    header = header\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "/// @brief " + e[1] + ": " + e[0] + "\n"\
           + "///\n"\
           + wrap(e[2], 80, 0, 0, "/// ").replace("<", "\<").replace(">", "\>") + "\n"\
           + "///\n"\
           + wrap(e[3], 80, 0, 0, "/// ") + "\n"\
           + "////////////////////////////////////////////////////////////////////////////////\n"\
           + "\n"\
           + "#define TRI_" + e[0].ljust(61) + " (" + e[1] + ")\n"\
           + "\n"

  header = header\
         + docend\
         + "#ifdef __cplusplus\n"\
         + "}\n"\
         + "#endif\n"\
         + "\n"\
         + "#endif\n"\
         + "\n"

  return header


# define some globals 
prologue = "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @brief auto-generated file generated from errors.dat\n"\
         + "////////////////////////////////////////////////////////////////////////////////\n"\
         + "\n"
  
docstart = "////////////////////////////////////////////////////////////////////////////////\n"\
         + "/// @addtogroup VocError\n"\
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
errors = csv.reader(open(source, "rb"))
errorsList = []

r1 = re.compile(r'^#.*')

for e in errors:
  if len(e) == 0:
    continue

  if r1.match(e[0]):
    continue

  if e[0] == "" or e[1] == "" or e[2] == "" or e[3] == "":
    print >> sys.stderr, "invalid error declaration file: %s (line %i)" % (source, i)
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
elif extension == ".c":
  out = genCFile(errorsList, filename)
else:
  print >> sys.stderr, "usage: %s <sourcefile> <outfile>" % sys.argv[0]
  sys.exit()

outFile = open(outfile, "wb")
outFile.write(out);
outFile.close()

