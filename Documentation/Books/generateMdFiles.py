import sys
import re
import os
import MarkdownPP

dokuBlocks = [{},{}]

def _mkdir_recursive(path):
    sub_path = os.path.dirname(path)
    if not os.path.exists(sub_path):
        _mkdir_recursive(sub_path)
    if not os.path.exists(path):
        os.mkdir(path)


def replaceCode(lines):
  lines = re.sub(r"<!--(\s*.+\s)-->","", lines)
  #HTTP API changing code
  lines = re.sub(r"@brief(.+)",r"\g<1>", lines)
  lines = re.sub(r"@RESTHEADER{([\s\w\/\_{}-]*),([\s\w-]*)}", r"###\g<2>\n`\g<1>`", lines)
  lines = re.sub(r"@RESTDESCRIPTION","", lines)
  lines = re.sub(r"@RESTURLPARAM(\s+)","**URL Parameters**\n", lines)
  lines = re.sub(r"@RESTQUERYPARAM(\s+)","**Query Parameters**\n", lines)
  lines = re.sub(r"@RESTHEADERPARAM(\s+)","**Header Parameters**\n", lines)
  lines = re.sub(r"@RESTBODYPARAM(\s+)","**Body Parameters**\n", lines)
  lines = re.sub(r"@RESTRETURNCODES","**Return Codes**\n", lines)
  lines = re.sub(r"@RESTURLPARAM", "@RESTPARAM", lines)
  lines = re.sub(r"@PARAMS", "**Parameters**\n", lines)
  lines = re.sub(r"@PARAM", "@RESTPARAM", lines)
  lines = re.sub(r"@RESTHEADERPARAM", "@RESTPARAM", lines)
  lines = re.sub(r"@RESTQUERYPARAM", "@RESTPARAM", lines)
  lines = re.sub(r"@RESTBODYPARAM", "@RESTPARAM", lines)
  lines = re.sub(r"@RESTPARAM{([\s\w\-]*),([\s\w\_\|-]*),\s[optional]}", r"* *\g<1>* (\g<3>):", lines)
  lines = re.sub(r"@RESTPARAM{([\s\w-]*),([\s\w\_\|-]*),\s*(\w+)}", r"* *\g<1>*:", lines)
  lines = re.sub(r"@RESTRETURNCODE{(.*)}", r"* *\g<1>*:", lines)
  lines = re.sub(r"@RESTBODYPARAMS{(.*)}", r"*(\g<1>)*", lines)
  lines = lines.replace("@EXAMPLES","**Examples**")
  lines = lines.replace("@RESTPARAMETERS","")
  lines = lines.replace("@RESTPARAMS","")
  # Error codes replace
  lines = re.sub(r"(####)#+", r"", lines)
#  lines = re.sub(r"- (\w+):\s*@LIT{(.+)}", r"\n*\g<1>* - **\g<2>**:", lines)
  lines = re.sub(r"(.+),(\d+),\"(.+)\",\"(.+)\"", r"\n*\g<2>* - **\g<3>**: \g<4>", lines)
  return lines

def replaceCodeIndex(lines):
  lines = re.sub(r"<!--(\s*.+\s)-->","", lines)
  #HTTP API changing code
  lines = re.sub(r"@brief(.+)",r"\g<1>", lines)
  lines = re.sub(r"@RESTHEADER{([\s\w\/\_{}-]*),([\s\w-]*)}", r"###\g<2>\n`\g<1>`", lines)
  return lines

################################################################################
# main loop over all files
################################################################################
def walk_on_files(inDirPath, outDirPath):
    for root, dirs, files in os.walk(inDirPath):
        for file in files:
            if file.endswith(".mdpp"):
                inFileFull = os.path.join(root, file)
                outFileFull = os.path.join(outDirPath, re.sub(r'mdpp$', 'md', inFileFull));
                print "%s -> %s" % (inFileFull, outFileFull)
                _mkdir_recursive(os.path.join(outDirPath, root))
                mdpp = open(inFileFull, "r")
                md = open(outFileFull, "w")
                MarkdownPP.MarkdownPP(input=mdpp, output=md, modules=MarkdownPP.modules.keys())
                mdpp.close()
                md.close()
                findStartCode(md, outFileFull);

def findStartCode(fd,full_path):
    inFD = open(full_path, "r")
    textFile =inFD.read()
    inFD.close()
    #print "-" * 80
    #print textFile
    matchInline = re.findall(r'@startDocuBlockInline\s*(\w+)', textFile)
    if matchInline:
        for find in matchInline:
            #print "7"*80
            print full_path + " " + find
            textFile = replaceTextInline(textFile, full_path, find)
            #print textFile

    match = re.findall(r'@startDocuBlock\s*(\w+)', textFile)
    if match:      
        for find in match:
            #print "8"*80
            textFile = replaceText(textFile, full_path, find)
            #print textFile

    textFile = replaceCodeIndex(textFile)
    textFile = replaceCode(textFile)
    #print "9" * 80
    #print textFile
    outFD = open(full_path, "w")

    outFD.truncate()
    outFD.write(textFile)
    outFD.close()


def replaceText(text, pathOfFile, searchText):
  ''' reads the mdpp and generates the md '''
  global dokuBlocks
  if not searchText in dokuBlocks[0]:
      print "Failed to locate the docublock '%s' for replacing it into the file '%s'\n have:" % (searchText, pathOfFile)
      print dokuBlocks[0].keys()
      print '*' * 80
      print text
      exit(1)
  rc= re.sub("@startDocuBlock\s+"+ searchText + "(?:\s+|$)", dokuBlocks[0][searchText], text)
  return rc

def replaceTextInline(text, pathOfFile, searchText):
  ''' reads the mdpp and generates the md '''
  global dokuBlocks
  if not searchText in dokuBlocks[1]:
      print "Failed to locate the inline docublock '%s' for replacing it into the file '%s'\n have:" % (searchText, pathOfFile)
      print dokuBlocks[1].keys()
      print '*' * 80
      print text
      exit(1)
  rePattern = r'\s*@startDocuBlockInline\s+'+ searchText +'.*@endDocuBlock\s' + searchText
  match = re.search(rePattern, text, flags=re.DOTALL);

  if (match == None): 
      print "failed to match with '%s' for %s in file %s in: \n%s" % (rePattern, searchText, pathOfFile, text)
      exit(1)

  subtext = match.group(0)
  if (len(re.findall('@startDocuBlock', subtext)) > 1):
      print "failed to snap with '%s' on end docublock for %s in %s our match is:\n%s" % (rePattern, searchText, pathOfFile, subtext)
      exit(1);

  return re.sub(rePattern, dokuBlocks[1][searchText], text, flags=re.DOTALL)

################################################################################
# Read the docublocks into memory
################################################################################
thisBlock = ""
thisBlockName = ""
thisBlockType = 0

STATE_SEARCH_START = 0
STATE_SEARCH_END = 1
def readStartLine(line):
    global thisBlockName, thisBlockType, thisBlock, dokuBlocks
    if ("@startDocuBlock" in line):
        if "@startDocuBlockInline" in line:
            thisBlockType = 1
        else:
            thisBlockType = 0
        try:
            thisBlockName = re.search(r" *start[0-9a-zA-Z]*\s\s*([0-9a-zA-Z_ ]*)\s*$", line).group(1).strip()
        except:
            print "failed to read startDocuBlock: [" + line + "]"
            exit(1)
        dokuBlocks[thisBlockType][thisBlockName] = ""
        return STATE_SEARCH_END
    return STATE_SEARCH_START

def readNextLine(line):
    global thisBlockName, thisBlockType, thisBlock, dokuBlocks
    if '@endDocuBlock' in line:
        return STATE_SEARCH_START
    dokuBlocks[thisBlockType][thisBlockName] += line
    #print "reading " + thisBlockName
    #print dokuBlocks[thisBlockType][thisBlockName]
    return STATE_SEARCH_END

def loadDokuBlocks():
    state = STATE_SEARCH_START
    f=open("allComments.txt", 'rU')
    count = 0;
    for line in f.readlines():
        if state == STATE_SEARCH_START:
            state = readStartLine(line)
        elif state == STATE_SEARCH_END:
            state = readNextLine(line)

        #if state == STATE_SEARCH_START:
        #    print dokuBlocks[thisBlockType].keys()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("usage: input-directory output-directory")
        exit(1)
        
    inDir = sys.argv[1]
    outDir = sys.argv[2]
    loadDokuBlocks()
    print "loaded %d / %d docu blocks" % (len(dokuBlocks[0]), len(dokuBlocks[1]))
    #print dokuBlocks[0].keys()
    walk_on_files(inDir, outDir)
