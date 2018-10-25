import sys
import re
import os
import json
import io
import shutil

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

################################################################################
### Swagger Markdown rendering
################################################################################
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
    swaggerDataTypes = ["number", "integer", "string", "boolean", "array", "object"]
    ####
    # print >>sys.stderr, "xx" * layer + reference
    global swagger
    rc = ''
    if not 'properties' in swagger['definitions'][reference]:
        if 'items' in swagger['definitions'][reference]:
            if swagger['definitions'][reference]['type'] == 'array':
                rc += '[\n'
            subStructRef = getReference(swagger['definitions'][reference]['items'], reference, None)
            rc += unwrapPostJson(subStructRef, layer + 1)
            if swagger['definitions'][reference]['type'] == 'array':
                rc += ']\n'
    else:
        for param in swagger['definitions'][reference]['properties'].keys():
            thisParam = swagger['definitions'][reference]['properties'][param]
            required = ('required' in swagger['definitions'][reference] and
                        param in swagger['definitions'][reference]['required'])
    
            # print >> sys.stderr, thisParam
            if '$ref' in thisParam:
                subStructRef = getReference(thisParam, reference, None)
    
                rc += '  ' * layer + "- **" + param + "**:\n"
                ####
                # print >>sys.stderr, "yy" * layer + param
                rc += unwrapPostJson(subStructRef, layer + 1)
        
            elif thisParam['type'] == 'object':
                rc += '  ' * layer + "- **" + param + "**: " + TrimThisParam(brTrim(thisParam['description']), layer) + "\n"
            elif thisParam['type'] == 'array':
                rc += '  ' * layer + "- **" + param + "**"
                trySubStruct = False
                lf=""
                ####
                # print >>sys.stderr, "zz" * layer + param
                if 'type' in thisParam['items']:
                    rc += " (" + thisParam['items']['type']  + ")"
                    lf="\n"
                else:
                    if len(thisParam['items']) == 0:
                        rc += " (anonymous json object)"
                        lf="\n"
                    else:
                        trySubStruct = True
                rc += ": " + TrimThisParam(brTrim(thisParam['description']), layer) + lf
                if trySubStruct:
                    try:
                        subStructRef = getReference(thisParam['items'], reference, None)
                    except:
                        print >>sys.stderr, ERR_COLOR + "while analyzing: " + param + RESET
                        print >>sys.stderr, WRN_COLOR + thisParam + RESET
                    rc += "\n" + unwrapPostJson(subStructRef, layer + 1)
            else:
                if thisParam['type'] not in swaggerDataTypes:
                    print >>sys.stderr, ERR_COLOR + "while analyzing: " + param + RESET
                    print >>sys.stderr, WRN_COLOR + thisParam['type'] + " is not a valid swagger datatype; supported ones: " + str(swaggerDataTypes) + RESET
                    raise Exception("invalid swagger type")
                rc += '  ' * layer + "- **" + param + "**: " + TrimThisParam(thisParam['description'], layer) + '\n'
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
        description = thisVerb['description']
        #print >> sys.stderr, description
        description = RX4[0].sub(RX4[1], description)
        return RX3[0].sub(RX3[1], description)
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


def noValidation():
    pass

def validatePathParameters():
    # print thisVerb
    for nParam in range(0, len(thisVerb['parameters'])):
        if thisVerb['parameters'][nParam]['in'] == 'path':
            break
    else:
        raise Exception("@RESTPATHPARAMETERS found in Swagger data without any parameter following in %s " % json.dumps(thisVerb, indent=4, separators=(', ',': '), sort_keys=True))

def validateQueryParams():
    # print thisVerb
    for nParam in range(0, len(thisVerb['parameters'])):
        if thisVerb['parameters'][nParam]['in'] == 'query':
            break
    else:
        raise Exception("@RESTQUERYPARAMETERS found in Swagger data without any parameter following in %s " % json.dumps(thisVerb, indent=4, separators=(', ',': '), sort_keys=True))

def validateHeaderParams():
    # print thisVerb
    for nParam in range(0, len(thisVerb['parameters'])):
        if thisVerb['parameters'][nParam]['in'] == 'header':
            break
    else:
        raise Exception("@RESTHEADERPARAMETERS found in Swagger data without any parameter following in %s " % json.dumps(thisVerb, indent=4, separators=(', ',': '), sort_keys=True))

def validateReturnCodes():
    # print thisVerb
    for nParam in range(0, len(thisVerb['responses'])):
        if len(thisVerb['responses'].keys()) != 0:
            break
    else:
        raise Exception("@RESTRETURNCODES found in Swagger data without any documented returncodes %s " % json.dumps(thisVerb, indent=4, separators=(', ',': '), sort_keys=True))

def validateExamples():
    pass

SIMPL_REPL_VALIDATE_DICT = {
    "@RESTDESCRIPTION"      : noValidation,
    "@RESTURLPARAMETERS"    : validatePathParameters,
    "@RESTQUERYPARAMETERS"  : validateQueryParams,
    "@RESTHEADERPARAMETERS" : validateHeaderParams,
    "@RESTRETURNCODES"      : validateReturnCodes,
    "@RESTURLPARAMS"        : validatePathParameters,
    "@EXAMPLES"             : validateExamples
}
SIMPL_REPL_DICT = {
    "\\"                    : "\\\\",
    "@HINTS"                : "",
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
@HINTS|                             # -> <inject hints>
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
    n = None
    try:
        n = SIMPL_REPL_VALIDATE_DICT[m]
    except:
        True
    if n != None:
        n()
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

RX3 = (re.compile(r'\*\*Example:\*\*((?:.|\n)*?)</code></pre>'), r"")

RX4 = (re.compile(r'<!-- Hints Start -->.*<!-- Hints End -->', re.DOTALL), r"")

match_RESTHEADER = re.compile(r"@RESTHEADER\{(.*)\}")
match_RESTRETURNCODE = re.compile(r"@RESTRETURNCODE\{(.*)\}")
have_RESTBODYPARAM = re.compile(r"@RESTBODYPARAM|@RESTDESCRIPTION")
have_RESTREPLYBODY = re.compile(r"@RESTREPLYBODY")
have_RESTSTRUCT = re.compile(r"@RESTSTRUCT")
remove_MULTICR = re.compile(r'\n\n\n*')

RXIMAGES = re.compile(r".*\!\[([\d\s\w\/\. ()-]*)\]\(([\d\s\w\/\.-]*)\).*")

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
                while ((len(lineR[r]) == 0) or
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
    try:
        lines = SIMPLE_RX.sub(SimpleRepl, lines)
    except Exception as x:
        print >> sys.stderr, ERR_COLOR + "While working on: [" + verb + " " + route + "]" + " while analysing " + blockName + RESET
        print >> sys.stderr, WRN_COLOR + x.message + RESET
        print >> sys.stderr, "Did you forget to run utils/generateSwagger.sh?"
        raise


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
                nextInFileFull = os.path.join(root, file)
                nextOutFileFull = os.path.join(outDirPath, nextInFileFull)
                if fileFilter != None:
                    if fileFilter.match(nextInFileFull) == None:
                        skipped += 1
                        # print "Skipping %s -> %s" % (inFileFull, outFileFull)
                        continue;
                #print "%s -> %s" % (nextInFileFull, nextOutFileFull)
                _mkdir_recursive(os.path.join(outDirPath, root))
                findStartCode(nextInFileFull, nextOutFileFull, inDirPath)
    print STD_COLOR + "Processed %d files, skipped %d" % (count, skipped) + RESET

def findStartCode(inFileFull, outFileFull, baseInPath):
    inFD = io.open(inFileFull, "r", encoding="utf-8", newline=None)
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
    
    def analyzeImages(m):
        imageLink = m.groups()[1]
        inf = os.path.realpath(os.path.join(os.path.dirname(inFileFull), imageLink))
        outf = os.path.realpath(os.path.join(os.path.dirname(outFileFull), imageLink))
        bookDir = os.path.realpath(baseInPath)
        depth = len(inFileFull.split(os.sep)) - 1 # filename + book directory
        assets = os.path.join((".." + os.sep)*depth, baseInPath, "assets")
	# print(inf, outf, bookDir, depth, assets)
        
        outdir = os.path.dirname(outf)
        if not os.path.exists(outdir):
            _mkdir_recursive(outdir)
        if os.path.commonprefix([inf, bookDir]) != bookDir:
            assetDir = os.path.join(outdir, assets)
            if not os.path.exists(assetDir):
                os.mkdir(assetDir)
            outf=os.path.join(assetDir, os.path.basename(imageLink))
            imageLink = os.path.join((".." + os.sep)* (depth - 1), "assets",os.path.basename(imageLink))

        if not os.path.exists(outf):
            shutil.copy(inf, outf)
        return str('![' + m.groups()[0] + '](' + imageLink + ')')

    textFile = re.sub(RXIMAGES,analyzeImages, textFile)
    outFD = io.open(outFileFull, "w", encoding="utf-8", newline="")
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
    f = io.open("allComments.txt", "r", encoding="utf-8", newline=None)
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

def loadProgramOptionBlocks():
    from itertools import groupby, chain
    from cgi import escape
    from glob import glob

    global dokuBlocks

    # Allows to test if a group will be empty with hidden options ignored
    def peekIterator(iterable, condition):
        try:
            while True:
                first = next(iterable)
                if condition(first):
                    break
        except StopIteration:
            return None
        return first, chain([first], iterable)

    # Give options a the section name 'global' if they don't have one
    def groupBySection(elem):
        return elem[1]["section"] or 'global'

    # Empty section string means global option, which should appear first
    def sortBySection(elem):
        section = elem[1]["section"]
        if section:
            return (1, section)
        return (0, u'global')

    # Format possible values as unordered list
    def formatList(arr, text=''):
        formatItem = lambda elem: '<li><code>{}</code></li>'.format(elem)
        return '{}<ul>{}</ul>\n'.format(text, '\n'.join(map(formatItem, arr)))

    for programOptionsDump in glob(os.path.normpath('../Examples/*.json')):

        program = os.path.splitext(os.path.basename(programOptionsDump))[0]
        output = []

        # Load program options dump and convert to Python object
        with io.open(programOptionsDump, 'r', encoding='utf-8', newline=None) as fp:
            try:
                optionsRaw = json.load(fp)
            except ValueError as err:
                # invalid JSON
                print >>sys.stderr, ERR_COLOR + "Failed to parse program options json: '" + programOptionsDump + "' - to be used as: '" + program + "' - " + err.message + RESET
                raise err

        # Group and sort by section name, global section first
        for groupName, group in groupby(
                sorted(optionsRaw.items(), key=sortBySection),
                key=groupBySection):

            # Use some trickery to skip hidden options without consuming items from iterator
            groupPeek = peekIterator(group, lambda elem: elem[1]["hidden"] is False)
            if groupPeek is None:
                # Skip empty section to avoid useless headline (all options are hidden)
                continue

            # Output table header with column labels (one table per section)
            output.append('\n<h2>{} Options</h2>'.format(groupName.title()))
            if program in ['arangod']:
                output.append('\nAlso see <a href="{0}.html">{0} details</a>.'.format(groupName.title()))
            output.append('\n<table class="program-options"><thead><tr>')
            output.append('<th>{}</th><th>{}</th><th>{}</th>'.format('Name', 'Type', 'Description'))
            output.append('</tr></thead><tbody>')

            # Sort options by name and output table rows
            for optionName, option in sorted(groupPeek[1], key=lambda elem: elem[0]):

                # Skip options marked as hidden
                if option["hidden"]:
                    continue

                # Recover JSON syntax, because the Python representation uses [u'this format']
                default = json.dumps(option["default"])

                # Parse and re-format the optional field for possible values
                # (not fully safe, but ', ' is unlikely to occur in strings)
                try:
                    optionList = option["values"].partition('Possible values: ')[2].split(', ')
                    values = formatList(optionList, '<br/>Possible values:\n')
                except KeyError:
                    values = ''

                # Expected data type for argument
                valueType = option["type"]

                # Enterprise Edition has EE only options marked
                enterprise = ""
                if option.setdefault("enterpriseOnly", False):
                    enterprise = "<em>Enterprise Edition only</em><br/>"

                # Upper-case first letter, period at the end, HTML entities
                description = option["description"].strip()
                description = description[0].upper() + description[1:]
                if description[-1] != '.':
                    description += '.'
                description = escape(description)

                # Description, default value and possible values separated by line breaks
                descriptionCombined = '\n'.join([enterprise, description, '<br/>Default: <code>{}</code>'.format(default), values])

                output.append('<tr><td><code>{}</code></td><td>{}</td><td>{}</td></tr>'.format(optionName, valueType, descriptionCombined))

            output.append('</tbody></table>')

        # Join output and register as docublock (like 'program_options_arangosh')
        dokuBlocks[0]['program_options_' + program.lower()] = '\n'.join(output) + '\n\n'

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
    f = io.open(swaggerJson, 'r', encoding='utf-8', newline=None)
    swagger= json.load(f)
    f.close()
    loadDokuBlocks()
    loadProgramOptionBlocks()
    print "%sloaded %d / %d docu blocks%s" % (STD_COLOR, len(dokuBlocks[0]), len(dokuBlocks[1]), RESET)
    #print dokuBlocks[0].keys()
    walk_on_files(inDir, outDir)
