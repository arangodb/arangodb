import sys
import re
import os
import json


RESET  = '\033[0m'
def make_std_color(No):
    # defined for 1 through 7
    return '\033[3' + No+ 'm'
def make_color(No):
    # defined for 1 through 255
    return '\033[38;5;'+ No + 'm'

WRN_COLOR = make_std_color('3')
ERR_COLOR = make_std_color('1')
STD_COLOR = make_color('8')

################################################################################
### @brief length of the swagger definition namespace
################################################################################

defLen = len('#/definitions/')

################################################################################
### @brief facility to remove leading and trailing html-linebreaks
################################################################################
removeTrailingBR = re.compile("<br>$")
removeLeadingBR = re.compile("^<br>")

def brTrim(text):
    return removeLeadingBR.sub("", removeTrailingBR.sub("", text.strip(' ')))

swagger = None
fileFilter = None
blockFilter = None
dokuBlocks = [{},{}]
thisVerb = {}
route = ''
verb = ''

def getReference(name, source, verb):
    try:
        ref = name['$ref'][defLen:]
    except Exception as x:
        print >>sys.stderr, ERR_COLOR + "No reference in: " + name + RESET
        raise
    if not ref in swagger['definitions']:
        fn = ''
        if verb:
            fn = swagger['paths'][route][verb]['x-filename']
        else:
            fn = swagger['definitions'][source]['x-filename']
        print >> sys.stderr, STD_COLOR + json.dumps(swagger['definitions'], indent=4, separators=(', ',': '), sort_keys=True) + RESET
        raise Exception("invalid reference: " + ref + " in " + fn)
    return ref

removeDoubleLF = re.compile("\n\n")
removeLF = re.compile("\n")

def TrimThisParam(text, indent):
    text = text.rstrip('\n').lstrip('\n')
    text = removeDoubleLF.sub("\n", text)
    if (indent > 0):
        indent = (indent + 2) # align the text right of the list...
    return removeLF.sub("\n" + ' ' * indent, text)

def unwrapPostJson(reference, layer):
    global swagger
    rc = ''
    for param in swagger['definitions'][reference]['properties'].keys():
        thisParam = swagger['definitions'][reference]['properties'][param]
        required = ('required' in swagger['definitions'][reference] and
                    param in swagger['definitions'][reference]['required'])

        if '$ref' in thisParam:
            subStructRef = getReference(thisParam, reference, None)

            rc += ' ' * layer + " - **" + param + "**:\n"
            rc += unwrapPostJson(subStructRef, layer + 1)
    
        elif thisParam['type'] == 'object':
            rc += ' ' * layer + " - **" + param + "**: " + TrimThisParam(brTrim(thisParam['description']), layer) + "\n"
        elif swagger['definitions'][reference]['properties'][param]['type'] == 'array':
            rc += ' ' * layer + " - **" + param + "**"
            trySubStruct = False
            if 'type' in thisParam['items']:
                rc += " (" + thisParam['items']['type']  + ")"
            else:
                if len(thisParam['items']) == 0:
                    rc += " (anonymous json object)"
                else:
                    trySubStruct = True
            rc += ": " + TrimThisParam(brTrim(thisParam['description']), layer)
            if trySubStruct:
                try:
                    subStructRef = getReference(thisParam['items'], reference, None)
                except:
                    print >>sys.stderr, ERR_COLOR + "while analyzing: " + param  + RESET
                    print >>sys.stderr, WRN_COLOR + thisParam + RESET
                rc += "\n" + unwrapPostJson(subStructRef, layer + 1)
        else:
            rc += ' ' * layer + " - **" + param + "**: " + TrimThisParam(thisParam['description'], layer) + '\n'
    return rc

def getRestBodyParam():
    rc = "\n**Body Parameters**\n"
    addText = ''
    for nParam in range(0, len(thisVerb['parameters'])):
        if thisVerb['parameters'][nParam]['in'] == 'body':
            descOffset = thisVerb['parameters'][nParam]['x-description-offset']
            addText = ''
            if 'additionalProperties' not in thisVerb['parameters'][nParam]['schema']:
                addText = unwrapPostJson(
                    getReference(thisVerb['parameters'][nParam]['schema'], route, verb),0)
    rc += addText
    return rc

def getRestDescription():
    #print >>sys.stderr, "RESTDESCRIPTION"
    if thisVerb['description']:
        #print >> sys.stderr, thisVerb['description']
        return thisVerb['description']
    else:
        #print >> sys.stderr, "ELSE"
        return ""
        
def getRestReplyBodyParam(param):
    rc = "\n**Response Body**\n"

    try:
        rc += unwrapPostJson(getReference(thisVerb['responses'][param]['schema'], route, verb), 0)
    except Exception:
        print >>sys.stderr, ERR_COLOR + "failed to search " + param + " in: " + RESET
        print >>sys.stderr, WRN_COLOR + json.dumps(thisVerb, indent=4, separators=(', ',': '), sort_keys=True) + RESET
        raise
    return rc + "\n"


SIMPL_REPL_DICT = {
    "\\"                    : "\\\\",
    "@RESTDESCRIPTION"      : getRestDescription,
    "@RESTURLPARAMETERS"    : "\n**Path Parameters**\n",
    "@RESTQUERYPARAMETERS"  : "\n**Query Parameters**\n",
    "@RESTHEADERPARAMETERS" : "\n**Header Parameters**\n",
    "@RESTRETURNCODES"      : "\n**Return Codes**\n",
    "@PARAMS"               : "\n**Parameters**\n",
    "@RESTPARAMS"           : "",
    "@RESTURLPARAMS"        : "\n**Path Parameters**\n",
    "@RESTQUERYPARAMS"      : "\n**Query Parameters**\n",
    "@RESTBODYPARAM"        : "", #getRestBodyParam,
    "@RESTREPLYBODY"        : getRestReplyBodyParam,
    "@RESTQUERYPARAM"       : "@RESTPARAM",
    "@RESTURLPARAM"         : "@RESTPARAM",
    "@PARAM"                : "@RESTPARAM",
    "@RESTHEADERPARAM"      : "@RESTPARAM",
    "@EXAMPLES"             : "\n**Examples**\n",
    "@RESTPARAMETERS"       : ""
}
SIMPLE_RX = re.compile(
r'''
\\|                                 # the backslash...
@RESTDESCRIPTION|                   # -> <empty>
@RESTURLPARAMETERS|                 # -> \n**Path Parameters**\n
@RESTQUERYPARAMETERS|               # -> \n**Query Parameters**\n
@RESTHEADERPARAMETERS|              # -> \n**Header Parameters**\n
@RESTBODYPARAM|                     # empty now, comes with the post body -> call post body param
@RESTRETURNCODES|                   # -> \n**Return Codes**\n
@PARAMS|                            # -> \n**Parameters**\n
@RESTPARAMS|                        # -> <empty>
@RESTURLPARAMS|                     # -> <empty>
@RESTQUERYPARAMS|                   # -> <empty>
@PARAM|                             # -> @RESTPARAM
@RESTURLPARAM|                      # -> @RESTPARAM
@RESTQUERYPARAM|                    # -> @RESTPARAM
@RESTHEADERPARAM|                   # -> @RESTPARAM
@EXAMPLES|                          # -> \n**Examples**\n
@RESTPARAMETERS|                    # -> <empty>
@RESTREPLYBODY\{(.*)\}              # -> call body function
''', re.X)


def SimpleRepl(match):
    m = match.group(0)
    # print 'xxxxx [%s]' % m
    try:
        n = SIMPL_REPL_DICT[m]
        if n == None:
            raise Exception("failed to find regex while searching for: " + m)
        else:
            if type(n) == type(''):
                return n
            else:
                return n()
    except Exception:
        pos = m.find('{')
        if pos > 0:
            newMatch = m[:pos]
            param = m[pos + 1 :].rstrip(' }')
            try:
                n = SIMPL_REPL_DICT[newMatch]
                if n == None:
                    raise Exception("failed to find regex while searching for: " +
                                    newMatch + " extracted from: " + m)
                else:
                    if type(n) == type(''):
                        return n
                    else:
                        return n(param)
            except Exception as x:
                #raise Exception("failed to find regex while searching for: " +
                #                newMatch + " extracted from: " + m)
                raise
        else:
            raise Exception("failed to find regex while searching for: " + m)

RX = [
    (re.compile(r"<!--(\s*.+\s)-->"), ""),
    # remove the placeholder BR's again
    (re.compile(r"<br />\n"), "\n"),
    # multi line bullet lists should become one
    (re.compile(r"\n\n-"), "\n-"),

    #HTTP API changing code
    # unwrap multi-line-briefs: (up to 3 lines supported by now ;-)
    (re.compile(r"@brief(.+)\n(.+)\n(.+)\n\n"), r"@brief\g<1> \g<2> \g<3>\n\n"),
    (re.compile(r"@brief(.+)\n(.+)\n\n"), r"@brief\g<1> \g<2>\n\n"),
    # if there is an @brief above a RESTHEADER, swap the sequence
    (re.compile(r"@brief(.+\n*)\n@RESTHEADER{([#\s\w\/\_{}-]*),([\s\w-]*)}"), r"###\g<3>\n\g<1>\n\n`\g<2>`"),
    # else simply put it into the text
    (re.compile(r"@brief(.+)"), r"\g<1>"),
    # there should be no RESTHEADER without brief, so we will fail offensively if by not doing
    #(re.compile(r"@RESTHEADER{([\s\w\/\_{}-]*),([\s\w-]*)}"), r"###\g<2>\n`\g<1>`"),

    # Format error codes from errors.dat
    (re.compile(r"#####+\n"), r""),
    (re.compile(r"## (.+\n\n)## (.+\n)"), r"## \g<1>\g<2>"),
    #  (re.compile(r"- (\w+):\s*@LIT{(.+)}"), r"\n*\g<1>* - **\g<2>**:"),
    (re.compile(r"(.+),(\d+),\"(.+)\",\"(.+)\""), r'\n* <a name="\g<1>"></a>**\g<2>** - **\g<1>**<br>\n  \g<4>'),

    (re.compile(r"TODOSWAGGER.*"),r"")
    ]


#    (re.compile(r"@RESTPARAM{([\s\w-]*),([\s\w\_\|-]*),\s*(\w+)}"), r"* *\g<1>*:"),
#    (re.compile(r"@RESTRETURNCODE{(.*)}"), r"* *\g<1>*:"),
#    (re.compile(r"@RESTBODYPARAMS{(.*)}"), r"*(\g<1>)*"),

RX2 = [
    # parameters - extract their type and whether mandatory or not.
    (re.compile(r"@RESTPARAM{(\s*[\w\-]*)\s*,\s*([\w\_\|-]*)\s*,\s*(required|optional)}"), r"* *\g<1>* (\g<3>):"),
    (re.compile(r"@RESTALLBODYPARAM{(\s*[\w\-]*)\s*,\s*([\w\_\|-]*)\s*,\s*(required|optional)}"), r"\n**Request Body** (\g<3>)\n\n"),

    (re.compile(r"@RESTRETURNCODE{(.*)}"), r"* *\g<1>*:")
]


match_RESTHEADER = re.compile(r"@RESTHEADER\{(.*)\}")
match_RESTRETURNCODE = re.compile(r"@RESTRETURNCODE\{(.*)\}")
have_RESTBODYPARAM = re.compile(r"@RESTBODYPARAM|@RESTDESCRIPTION")
have_RESTREPLYBODY = re.compile(r"@RESTREPLYBODY")
have_RESTSTRUCT = re.compile(r"@RESTSTRUCT")
remove_MULTICR = re.compile(r'\n\n\n*')

def _mkdir_recursive(path):
    sub_path = os.path.dirname(path)
    if not os.path.exists(sub_path):
        _mkdir_recursive(sub_path)
    if not os.path.exists(path):
        os.mkdir(path)


def replaceCode(lines, blockName):
    global swagger, thisVerb, route, verb
    thisVerb = {}
    foundRest = False
    # first find the header:
    headerMatch = match_RESTHEADER.search(lines)
    if headerMatch and headerMatch.lastindex > 0:
        foundRest = True
        try:
            (verb,route) =  headerMatch.group(1).split(',')[0].split(' ')
            verb = verb.lower()
        except:
            print >> sys.stderr, ERR_COLOR + "failed to parse header from: " + headerMatch.group(1) + " while analysing " + blockName + RESET
            raise

        try:
            thisVerb = swagger['paths'][route][verb]
        except:
            print >> sys.stderr, ERR_COLOR + "failed to locate route in the swagger json: [" + verb + " " + route + "]" + " while analysing " + blockName + RESET
            print >> sys.stderr, WRN_COLOR + lines + RESET
            print >> sys.stderr, "Did you forget to run utils/generateSwagger.sh?"
            raise

    for (oneRX, repl) in RX:
        lines = oneRX.sub(repl, lines)


    if foundRest:
        rcCode = None
        foundRestBodyParam = False
        foundRestReplyBodyParam = False
        lineR = lines.split('\n')
        #print lineR
        l = len(lineR)
        r = 0
        while (r < l): 
            # remove all but the first RESTBODYPARAM:
            if have_RESTBODYPARAM.search(lineR[r]):
                if foundRestBodyParam:
                    lineR[r] = ''
                else:
                    lineR[r] = '@RESTDESCRIPTION'
                foundRestBodyParam = True
                r+=1
                while ((len(lineR[r]) > 0) and
                       ((lineR[r][0] != '@') or
                       have_RESTBODYPARAM.search(lineR[r]))):
                    # print "xxx - %d %s" %(len(lineR[r]), lineR[r])
                    lineR[r] = ''
                    r+=1
    
            m = match_RESTRETURNCODE.search(lineR[r])
            if m and m.lastindex > 0:
                rcCode =  m.group(1)
            
            # remove all but the first RESTREPLYBODY:
            if have_RESTREPLYBODY.search(lineR[r]):
                if foundRestReplyBodyParam != rcCode:
                    lineR[r] = '@RESTREPLYBODY{' + rcCode + '}\n' 
                else:
                    lineR[r] = ''
                foundRestReplyBodyParam = rcCode
                r+=1
                while (len(lineR[r]) > 1):
                    lineR[r] = ''
                    r+=1
                m = match_RESTRETURNCODE.search(lineR[r])
                if m and m.lastindex > 0:
                    rcCode =  m.group(1)
    
            # remove all RESTSTRUCTS - they're referenced anyways:
            if have_RESTSTRUCT.search(lineR[r]):
                while (len(lineR[r]) > 1):
                    lineR[r] = ''
                    r+=1
            r+=1
        lines = "\n".join(lineR)
    #print "x" * 70
    #print lines
    lines = SIMPLE_RX.sub(SimpleRepl, lines)

    for (oneRX, repl) in RX2:
        lines = oneRX.sub(repl, lines)

    lines = remove_MULTICR.sub("\n\n", lines)
    #print lines
    return lines

def replaceCodeIndex(lines):
  lines = re.sub(r"<!--(\s*.+\s)-->","", lines)
  #HTTP API changing code
  #lines = re.sub(r"@brief(.+)",r"\g<1>", lines)
  #lines = re.sub(r"@RESTHEADER{([\s\w\/\_{}-]*),([\s\w-]*)}", r"###\g<2>\n`\g<1>`", lines)
  return lines

RXUnEscapeMDInLinks = re.compile("\\\\_")
def setAnchor(param):
    unescapedParam = RXUnEscapeMDInLinks.sub("_", param)
    return "<a name=\"" + unescapedParam + "\">#</a>" 

RXFinal = [
    (re.compile(r"@anchor (.*)"), setAnchor),
]
def replaceCodeFullFile(lines):
    for (oneRX, repl) in RXFinal:
        lines = oneRX.sub(repl, lines)
    return lines

################################################################################
# main loop over all files
################################################################################
def walk_on_files(inDirPath, outDirPath):
    global fileFilter
    count = 0
    skipped = 0
    for root, dirs, files in os.walk(inDirPath):
        for file in files:
            if file.endswith(".md") and not file.endswith("SUMMARY.md"):
                count += 1
                inFileFull = os.path.join(root, file)
                outFileFull = os.path.join(outDirPath, inFileFull)
                if fileFilter != None:
                    if fileFilter.match(inFileFull) == None:
                        skipped += 1
                        # print "Skipping %s -> %s" % (inFileFull, outFileFull)
                        continue;
                # print "%s -> %s" % (inFileFull, outFileFull)
                _mkdir_recursive(os.path.join(outDirPath, root))
                findStartCode(inFileFull, outFileFull)
    print STD_COLOR + "Processed %d files, skipped %d" % (count, skipped) + RESET

def findStartCode(inFileFull, outFileFull):
    inFD = open(inFileFull, "r")
    textFile = inFD.read()
    inFD.close()
    #print "-" * 80
    #print textFile
    matchInline = re.findall(r'@startDocuBlockInline\s*(\w+)', textFile)
    if matchInline:
        for find in matchInline:
            #print "7"*80
            #print inFileFull + " " + find
            textFile = replaceTextInline(textFile, inFileFull, find)
            #print textFile

    match = re.findall(r'@startDocuBlock\s*(\w+)', textFile)
    if match:
        for find in match:
            #print "8"*80
            #print find
            textFile = replaceText(textFile, inFileFull, find)
            #print textFile

    try:
        textFile = replaceCodeFullFile(textFile)
    except:
        print >>sys.stderr, ERR_COLOR + "while parsing :      "  + inFileFull + RESET
        raise
    #print "9" * 80
    #print textFile
    outFD = open(outFileFull, "w")
    outFD.write(textFile)
    outFD.close()
#JSF_put_api_replication_synchronize

def replaceText(text, pathOfFile, searchText):
  ''' inserts docublocks into md '''
  #print '7'*80
  global dokuBlocks
  if not searchText in dokuBlocks[0]:
      print >> sys.stderr, ERR_COLOR + "Failed to locate the docublock '" + searchText + "' for replacing it into the file '" +pathOfFile + "'\n have:" + RESET
      print >> sys.stderr, WRN_COLOR + dokuBlocks[0].keys() + RESET
      print >> sys.stderr, ERR_COLOR + '*' * 80 + RESET
      print >> sys.stderr, WRN_COLOR + text + RESET
      print >> sys.stderr, ERR_COLOR + "Failed to locate the docublock '" + searchText + "' for replacing it into the file '" +pathOfFile + "' For details scroll up!" + RESET
      exit(1)
  #print '7'*80
  #print dokuBlocks[0][searchText]
  #print '7'*80
  rc= re.sub("@startDocuBlock\s+"+ searchText + "(?:\s+|$)", dokuBlocks[0][searchText], text)
  return rc

def replaceTextInline(text, pathOfFile, searchText):
  ''' inserts docublocks into md '''
  global dokuBlocks
  if not searchText in dokuBlocks[1]:
      print >> sys.stderr, ERR_COLOR + "Failed to locate the inline docublock '" + searchText + "' for replacing it into the file '" + pathOfFile + "'\n have: " + RESET
      print >> sys.stderr, "%s%s%s" %(WRN_COLOR, dokuBlocks[1].keys(), RESET)
      print >> sys.stderr, ERR_COLOR + '*' * 80 + RESET
      print >> sys.stderr, WRN_COLOR + text + RESET
      print >> sys.stderr, ERR_COLOR + "Failed to locate the inline docublock '" + searchText + "' for replacing it into the file '" + pathOfFile + "' For details scroll up!" + RESET
      exit(1)
  rePattern = r'(?s)\s*@startDocuBlockInline\s+'+ searchText +'\s.*?@endDocuBlock\s' + searchText
  # (?s) is equivalent to flags=re.DOTALL but works in Python 2.6
  match = re.search(rePattern, text)

  if (match == None):
      print >> sys.stderr, ERR_COLOR + "failed to match with '" + rePattern + "' for " + searchText + " in file " + pathOfFile + " in: \n" + text + RESET
      exit(1)

  subtext = match.group(0)
  if (len(re.findall('@startDocuBlock', subtext)) > 1):
      print >> sys.stderr, ERR_COLOR + "failed to snap with '" + rePattern + "' on end docublock for " + searchText + " in " + pathOfFile + " our match is:\n" + subtext + RESET
      exit(1)

  return re.sub(rePattern, dokuBlocks[1][searchText], text)

################################################################################
# Read the docublocks into memory
################################################################################
thisBlock = ""
thisBlockName = ""
thisBlockType = 0

STATE_SEARCH_START = 0
STATE_SEARCH_END = 1
SEARCH_START = re.compile(r" *start[0-9a-zA-Z]*\s\s*([0-9a-zA-Z_ ]*)\s*$")


def readStartLine(line):
    global thisBlockName, thisBlockType, thisBlock, dokuBlocks
    if ("@startDocuBlock" in line):
        if "@startDocuBlockInline" in line:
            thisBlockType = 1
        else:
            thisBlockType = 0
        try:
            thisBlockName = SEARCH_START.search(line).group(1).strip()
        except:
            print >> sys.stderr, ERR_COLOR + "failed to read startDocuBlock: [" + line + "]"  + RESET
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
    count = 0
    for line in f.readlines():
        if state == STATE_SEARCH_START:
            state = readStartLine(line)
        elif state == STATE_SEARCH_END:
            state = readNextLine(line)

        #if state == STATE_SEARCH_START:
        #    print dokuBlocks[thisBlockType].keys()
        
    if blockFilter != None:
        remainBlocks= {}
        print STD_COLOR + "filtering blocks" + RESET
        for oneBlock in dokuBlocks[0]:
            if blockFilter.match(oneBlock) != None:
                print "%sfound block %s%s" % (STD_COLOR, oneBlock, RESET)
                #print dokuBlocks[0][oneBlock]
                remainBlocks[oneBlock] = dokuBlocks[0][oneBlock]
        dokuBlocks[0] = remainBlocks
        
    for oneBlock in dokuBlocks[0]:
        try:
            #print "processing %s" % oneBlock
            dokuBlocks[0][oneBlock] = replaceCode(dokuBlocks[0][oneBlock], oneBlock)
            #print "6"*80
            #print dokuBlocks[0][oneBlock]
            #print "6"*80
        except:
            print >>sys.stderr, ERR_COLOR + "while parsing :\n"  + oneBlock + RESET
            raise

    for oneBlock in dokuBlocks[1]:
        try:
            dokuBlocks[1][oneBlock] = replaceCode(dokuBlocks[1][oneBlock], oneBlock)
        except:
            print >>sys.stderr, WRN_COLOR + "while parsing :\n"  + oneBlock + RESET
            raise


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("usage: input-directory output-directory swaggerJson [filter]")
        exit(1)
    inDir = sys.argv[1]
    outDir = sys.argv[2]
    swaggerJson = sys.argv[3]
    if len(sys.argv) > 4 and sys.argv[4].strip() != '':
        print STD_COLOR + "filtering " + sys.argv[4] + RESET
        fileFilter = re.compile(sys.argv[4])
    if len(sys.argv) > 5 and sys.argv[5].strip() != '':
        print STD_COLOR + "filtering Docublocks: " + sys.argv[5] + RESET
        blockFilter = re.compile(sys.argv[5])
    f=open(swaggerJson, 'rU')
    swagger= json.load(f)
    f.close()
    loadDokuBlocks()
    print "%sloaded %d / %d docu blocks%s" % (STD_COLOR, len(dokuBlocks[0]), len(dokuBlocks[1]), RESET)
    #print dokuBlocks[0].keys()
    walk_on_files(inDir, outDir)
