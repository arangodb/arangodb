import sys
import re
import os

def walk_on_files(dirpath):
  for root, dirs, files in os.walk(dirpath):
    for file in files:
      if file.endswith(".html"):
        if file == "SUMMARY.md":
          pass
        elif file == "Index.md":
          pass
        else:  
          full_path= os.path.join(root, file)
          f=open(full_path,"rU")
          print "checking file: " + full_path
          textFile=f.read()
          f.close()
          replaceCode(full_path)
  return

def replaceCode(pathOfFile):
  f=open(pathOfFile,"rU")
  if f:
    s=f.read()
  f.close()
  f=open(pathOfFile,'w')
  lines = s.replace("<a href=\"../Blueprint-Graphs/README.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../Blueprint-Graphs/README.html\">")
  lines = lines.replace("<a href=\"../Blueprint-Graphs/VertexMethods.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../Blueprint-Graphs/VertexMethods.html\">")
  lines = lines.replace("<a href=\"../Blueprint-Graphs/GraphConstructor.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../Blueprint-Graphs/GraphConstructor.html\">")
  lines = lines.replace("<a href=\"../Blueprint-Graphs/EdgeMethods.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../Blueprint-Graphs/EdgeMethods.html\">")
  lines = lines.replace("<a href=\"../HttpGraphs/README.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../HttpGraphs/README.html\">")
  lines = lines.replace("<a href=\"../HttpGraphs/Vertex.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../HttpGraphs/Vertex.html\">")
  lines = lines.replace("<a href=\"../HttpGraphs/Edge.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../HttpGraphs/Edge.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/README.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../ModuleGraph/README.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/GraphConstructor.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../ModuleGraph/GraphConstructor.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/VertexMethods.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../ModuleGraph/VertexMethods.html\">")
  lines = lines.replace("<a href=\"../ModuleGraph/EdgeMethods.html\">","<a style=\"background-color:rgba(240,240,0,0.4)\" href=\"../ModuleGraph/EdgeMethods.html\">")
  f.write(lines)
  f.close()

if __name__ == '__main__':
    path = ["Documentation/Books/books/Users"]
    for i in path:
      dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))  
    walk_on_files(dirpath)