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
  replaced = re.sub("<!-- (\.*) -->","",replaced)

  # HTTP API changing code
  replaced = re.sub(r"@brief(.+)",r"\g<1>", replaced)
  replaced = re.sub(r"@RESTHEADER{([\s\w\/\_{}-]*),([\s\w-]*)}", r"###\g<2>\n `\g<1>`", replaced)
  replaced = replaced.replace("@RESTDESCRIPTION","")
  replaced = replaced.replace("@RESTURLPARAMETERS","**URL Parameters**\n")  
  replaced = replaced.replace("@RESTURLPARAMS","**URL Parameters**\n")
  replaced = replaced.replace("@RESTQUERYPARAMS","**Query Parameters**\n")
  replaced = replaced.replace("@RESTQUERYPARAMETERS","**Query Parameters**\n")  
  replaced = replaced.replace("@RESTHEADERPARAMS","**Header Parameters**\n")
  replaced = replaced.replace("@RESTHEADERPARAMETERS","**Header Parameters**\n")  
  replaced = replaced.replace("@RESTBODYPARAMS","**Body Parameters**\n")
  replaced = replaced.replace("@RESTBODYPARAMETERS","**Body Parameters**\n")  
  replaced = replaced.replace("@RESTRETURNCODES","**Return Codes**\n")
  replaced = replaced.replace("@RESTURLPARAM", "@RESTPARAM")
  replaced = replaced.replace("@PARAMS", "**Parameters**\n")
  replaced = replaced.replace("@PARAM", "@RESTPARAM")
  replaced = replaced.replace("@RESTHEADERPARAM", "@RESTPARAM")
  replaced = replaced.replace("@RESTQUERYPARAM", "@RESTPARAM")
  replaced = replaced.replace("@RESTBODYPARAM", "@RESTPARAM")
  replaced = re.sub(r"@RESTPARAM{([\s\w\-]*),([\s\w\_\|-]*),\s[optional]}", r"* *\g<1>* (\g<3>):", replaced)
  replaced = re.sub(r"@RESTPARAM{([\s\w-]*),([\s\w\_\|-]*),\s*(\w+)}", r"* *\g<1>*:", replaced)
  replaced = re.sub(r"@RESTRETURNCODE{(.*)}", r"* *\g<1>*:", replaced)
  replaced = re.sub(r"@RESTBODYPARAMS{(.*)}", r"*(\g<1>)*", replaced)
  replaced = replaced.replace("@EXAMPLES","**Examples**")
  # Error codes replace
<<<<<<< HEAD
  replaced = re.sub(r"- (\w+):\s*@LIT{(.+)}", r"\n*\g<1>* - **\g<2>**:", replaced)
  f.write(replaced)
=======
  lines = re.sub(r"(##)#+", r"", lines)
#  lines = re.sub(r"- (\w+):\s*@LIT{(.+)}", r"\n*\g<1>* - **\g<2>**:", lines)
  lines = re.sub(r"(.+),(\d+),\"(.+)\",\"(.+)\"", r"\n*\g<2>* - **\g<3>**: \g<4>", lines)
  f.write(lines)
  f.close()

def replaceCodeIndex(pathOfFile):
  f=open(pathOfFile,"rU")
  if f:
    s=f.read()
  f.close()
  f=open(pathOfFile,'w')
  lines = re.sub(r"<!--(\s*.+\s)-->","",s)
  #HTTP API changing code
  lines = re.sub(r"@brief(.+)",r"\g<1>", lines)
  lines = re.sub(r"@RESTHEADER{([\s\w\/\_{}-]*),([\s\w-]*)}", r"###\g<2>\n`\g<1>`", lines)
  f.write(lines)
>>>>>>> 370219d... Error Codes will be shown correctly in the documentation
  f.close()

        
if __name__ == '__main__':
    path = ["Documentation/Books/Users"]
    for i in path:
      dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))  
    walk_on_files(dirpath)