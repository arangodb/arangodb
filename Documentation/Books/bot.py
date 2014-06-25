import sys
import re
import os

def walk_on_files(dirpath):
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

def findStartCode(textFile,full_path):
  match = re.findall(r'@startDocuBlock\s*(\w+)', textFile)
  if match:      
    for find in match:
      textToReplace=getTextFromSourceFile(find, full_path)

def getTextFromSourceFile(searchText, full_path):
  f=open("allComments.txt", 'rU')
  s=f.read()
  match = re.search(r'@startDocuBlock\s+'+ searchText + "(?:\s+|$)" +'(.+?)@endDocuBlock', s,re.DOTALL)
  if match:
    textExtracted = match.group(1)
    textExtracted = textExtracted.replace("<br />","\n")
    replaceText(textExtracted, full_path, searchText)

def replaceText(text, pathOfFile, searchText):
  f=open(pathOfFile,"rU")
  if f:
    s=f.read()
  f.close()
  f=open(pathOfFile,'w')

  replaced = re.sub("@startDocuBlock\s+"+ searchText + "(?:\s+|$)",text,s)

  # HTTP API changing code
  replaced = replaced.replace("@brief","")
  replaced = re.sub(r"@RESTHEADER{([\s\w\/-]*),([\s\w]*)}", r"###\g<2>\n `\g<1>", replaced)
  replaced = replaced.replace("@RESTDESCRIPTION","")
  replaced = replaced.replace("@RESTURLPARAMS","*URL Parameters*\n")
  replaced = replaced.replace("@RESTQUERYPARAMS","*Query Parameters*\n")
  replaced = replaced.replace("@RESTHEADERPARAMS","*Header Parameters*\n")
  replaced = replaced.replace("@RESTBODYPARAMS","*Body Parameters*\n")
  replaced = replaced.replace("@RESTRETURNCODES","*Return Codes*")
  replaced = re.sub(r"@RESTPARAM{([\s\w]*),([\s\w]*),\s*(\w+)}", r"* *\g<1>* (\g<3>):", replaced)
  #  replaced = re.sub(r"@RESTPARAM{(\s*\w+\-*\w+),(\s*\w+),\s*(\w+)}", r"* *\g<1>* (\g<3>):", replaced)
  replaced = re.sub(r"@RESTRETURNCODE{(.*)}", r"* *HTTP \g<1>*:", replaced)
  replaced = re.sub(r"@RESTBODYPARAMS{(.*)}", r"*(\g<1>)*", replaced)
  replaced = replaced.replace("@EXAMPLES","*Examples*")

  f.write(replaced)
  f.close()

        
if __name__ == '__main__':
    path = ["Documentation/Books/Users"]
    for i in path:
      dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))  
    walk_on_files(dirpath)