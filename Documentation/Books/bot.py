import sys
import re
import os

def replaceText(text,pathOfFile):
  f=open(pathOfFile,"rU")
  if f:
    s=f.read()
  f.close()
  f=open(pathOfFile,'w')

  replaced=re.sub('@startDocuBlock\s+\w+',text,s)

  f.write(replaced)
  f.close()

def getTextFromSourceFile(searchText):
  f=open("allComments.txt", 'rU')
  s=f.read()
  match = re.search(r'@startDocuBlock\s+'+re.escape(searchText)+'(.+?)@endDocuBlock', s,re.DOTALL)
  if match:
    global textExtracted
    textExtracted = match.group(1)
  return textExtracted
def findStartCode(textFile,full_path):
  match = re.findall(r'@startDocuBlock\s*(\w+)', textFile)
  if match:      
    for find in match:
      textToReplace=getTextFromSourceFile(find)
      replaceText(textToReplace,full_path)
  return 
  
def walk_on_files(dirpatp):
  for root, dirs, files in os.walk(dirpath):
    for file in files:
      if file.endswith(".md"):
        full_path= os.path.join(root, file)
        f=open(full_path,"rU")
        print "checking file: " + full_path
        textFile=f.read()
        f.close()
        findStartCode(textFile,full_path)
  return

def main():
  walk_on_files()
  
if __name__ == '__main__':
    path = ["Documentation/Books/Users/"]
    for i in path:
      dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))  
    walk_on_files(dirpath)