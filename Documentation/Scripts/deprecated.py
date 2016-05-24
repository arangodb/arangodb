import sys
import re
import os

def walk_on_files(dirpath, newVersionNumber):
  for root, dirs, files in os.walk(dirpath):
    for file in files:
      if file.endswith(".html"):
        full_path= os.path.join(root, file)
        replaceCode(full_path, newVersionNumber)
  return

def replaceCode(pathOfFile, newVersionNumber):
  f=open(pathOfFile,"rU")
  if f:
    lines=f.read()
  f.close()
  f=open(pathOfFile,'w')
  #lines = lines.replace("!CHAPTER","#")
  lines = re.sub("!CHAPTER\s+" + "(.*)", r"<h1>\g<1></h1>", lines)
  #lines = lines.replace("!SECTION","##")
  lines = re.sub("!SECTION\s+" + "(.*)", r"<h2>\g<1></h2>", lines)
  #lines = lines.replace("!SUBSECTION","###")
  lines = re.sub("!SUBSECTION\s+" + "(.*)", r"<h3>\g<1></h3>", lines)
  #lines = lines.replace("!SUBSUBSECTION","####")
  lines = re.sub("!SUBSUBSECTION\s+" + "(.*)", r"<h4>\g<1></h4>", lines)
  lines = lines.replace("VERSION_NUMBER", newVersionNumber)
  f.write(lines)
  f.close()

if __name__ == '__main__':
  path = ["Documentation/Books/books/Manual"]
  g = open(os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir, "ArangoDB/../../VERSION")))
  if g:
    newVersionNumber=g.read()
  g.close
  for i in path:
    dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))
    print "Tagging deprecated files"
    walk_on_files(dirpath, newVersionNumber)
