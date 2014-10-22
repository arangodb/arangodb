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
    s=f.read()
  f.close()
  f=open(pathOfFile,'w')
  lines = s.replace("<a href=\"../Blueprint-Graphs/README.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../Blueprint-Graphs/README.html\">")
  lines = lines.replace("<a href=\"../Blueprint-Graphs/VertexMethods.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../Blueprint-Graphs/VertexMethods.html\">")
  lines = lines.replace("<a href=\"../Blueprint-Graphs/GraphConstructor.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../Blueprint-Graphs/GraphConstructor.html\">")
  lines = lines.replace("<a href=\"../Blueprint-Graphs/EdgeMethods.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../Blueprint-Graphs/EdgeMethods.html\">")
  lines = lines.replace("<a href=\"../HttpGraphs/README.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../HttpGraphs/README.html\">")
  lines = lines.replace("<a href=\"../HttpGraphs/Vertex.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../HttpGraphs/Vertex.html\">")
  lines = lines.replace("<a href=\"../HttpGraphs/Edge.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../HttpGraphs/Edge.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/README.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../ModuleGraph/README.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/GraphConstructor.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../ModuleGraph/GraphConstructor.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/VertexMethods.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../ModuleGraph/VertexMethods.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/EdgeMethods.html\">","<a class=\"fa fa-exclamation-triangle\" style=\"color:rgba(240,210,0,1)\" href=\"../ModuleGraph/EdgeMethods.html\">")
  lines = lines.replace("VERSION_NUMBER", newVersionNumber)
  f.write(lines)
  f.close()

if __name__ == '__main__':
  path = ["Documentation/Books/books/Users"]
  g = open(os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir, "ArangoDB/../../VERSION")))
  if g:
    newVersionNumber=g.read()
  g.close
  for i in path:
    dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))
    print "Tagging deprecated files"
    walk_on_files(dirpath, newVersionNumber)