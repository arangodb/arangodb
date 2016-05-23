################################################################################
### DISCLAIMER
###
### Copyright by triAGENS GmbH - All rights reserved.
###
### The Programs (which include both the software and documentation)
### contain proprietary information of triAGENS GmbH; they are
### provided under a license agreement containing restrictions on use and
### disclosure and are also protected by copyright, patent and other
### intellectual and industrial property laws. Reverse engineering,
### disassembly or decompilation of the Programs, except to the extent
### required to obtain interoperability with other independently created
### software or as specified by law, is prohibited.
###
### The Programs are not intended for use in any nuclear, aviation, mass
### transit, medical, or other inherently dangerous applications. It shall
### be the licensee's responsibility to take all appropriate fail-safe,
### backup, redundancy, and other measures to ensure the safe use of such
### applications if the Programs are used for such purposes, and triAGENS
### GmbH disclaims liability for any damages caused by such use of
### the Programs.
###
### This software is the confidential and proprietary information of
### triAGENS GmbH. You shall not disclose such confidential and
### proprietary information and shall use it only in accordance with the
### terms of the license agreement you entered into with triAGENS GmbH.
###
### Copyright holder is triAGENS GmbH, Cologne, Germany
###
### @author Dr. Frank Celler
### @author Copyright 2011-2014, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, string, os, re
from pprint import pprint

################################################################################
### @brief enable debug output
################################################################################

DEBUG = False

################################################################################
### @brief enable debug output in JavaScript
################################################################################

JS_DEBUG = False

################################################################################
### @brief output directory
################################################################################

OutputDir = "/tmp/"

################################################################################
### @brief arangosh output
###
### A list of commands that are executed in order to produce the output. The
### commands and there output is logged.
################################################################################

RunTests = {}

################################################################################
### A list of tests that were skipped by the users request.
################################################################################

filterTestList = []

################################################################################
### @brief arangosh expect
###
### A list of commands that are here to validate the result.
################################################################################

ArangoshExpect = {}

################################################################################
### @brief arangosh run
###
### starting Line Numbers of ArangoshRun
################################################################################
ArangoshRunLineNo = {}

################################################################################
### @brief arangosh output files
################################################################################

ArangoshFiles = {}

################################################################################
### @brief map the source files for error messages
################################################################################

MapSourceFiles = {}

################################################################################
### @brief global setup for arangosh
################################################################################

ArangoshSetup = ""

################################################################################
### @brief filter to only output this one:
################################################################################

FilterForTestcase = None

################################################################################
### @brief states
################################################################################

STATE_BEGIN = 0
STATE_ARANGOSH_OUTPUT = 'HTTP_LOUTPUT'
STATE_ARANGOSH_RUN = 'ARANGOSH_OUTPUT'

################################################################################
### @brief option states
################################################################################

OPTION_NORMAL = 0
OPTION_ARANGOSH_SETUP = 1
OPTION_OUTPUT_DIR = 2
OPTION_FILTER = 3
OPTION_OUTPUT_FILE = 4

escapeBS = re.compile("\\\\")
doubleBS = "\\\\\\\\"

################################################################################
### @brief generate arangosh example headers with functions etc. needed later
################################################################################

def generateArangoshHeader():
    headerF = open("./Documentation/Scripts/exampleHeader.js", "r")
    print headerF.read()
    headerF.close()

################################################################################
### @brief Try to match the start of a command section
################################################################################

regularStartLine = re.compile(r'^(/// )? *@EXAMPLE_ARANGOSH_OUTPUT{([^}]*)}')
runLine = re.compile(r'^(/// )? *@EXAMPLE_ARANGOSH_RUN{([^}]*)}')
    
def matchStartLine(line, filename):
    global regularStartLine, errorStartLine, runLine, FilterForTestcase, filterTestList
    errorName = ""
    m = regularStartLine.match(line)

    if m:
        errorName = ""
        strip = m.group(1)
        name = m.group(2)

        if name in ArangoshFiles:
            print >> sys.stderr, "%s\nduplicate test name '%s' in file %s!\n%s\n" % ('#' * 80, name, filename, '#' * 80)
            sys.exit(1)

        # if we match for filters, only output these!
        if ((FilterForTestcase != None) and not FilterForTestcase.match(name)):
            filterTestList.append(name)
            return("", STATE_BEGIN);

        return (name, STATE_ARANGOSH_OUTPUT)

    m = runLine.match(line)

    if m:
        strip = m.group(1)
        name = m.group(2)
        
        if name in ArangoshFiles:
            print >> sys.stderr, "%s\nduplicate test name '%s' in file %s!\n%s\n" % ('#' * 80, name, filename, '#' * 80)
            sys.exit(1)

        # if we match for filters, only output these!
        if ((FilterForTestcase != None) and not FilterForTestcase.match(name)):
            filterTestList.append(name)
            return("", STATE_BEGIN);

        ArangoshFiles[name] = True
        return (name, STATE_ARANGOSH_RUN)

    # Not found, remain in STATE_BEGIN
    return ("", STATE_BEGIN)

endExample = re.compile(r'^(/// )? *@END_EXAMPLE_')
#r5 = re.compile(r'^ +')

TESTLINES="testlines"
TYPE="type"
LINE_NO="lineNo"
STRING="string"

################################################################################
### @brief loop over the lines of one input file
################################################################################

def analyzeFile(f, filename): 
    global RunTests, TESTLINES, TYPE, LINE_NO, STRING
    strip = None
    
    name = ""
    partialCmd = ""
    partialLine = ""
    partialLineStart = 0
    exampleStartLine = 0
    state = STATE_BEGIN
    lineNo = 0;

    for line in f:
        lineNo += 1
        if strip is None:
            strip = ""

        line = line.rstrip('\n')

        # read the start line and remember the prefix which must be skipped

        if state == STATE_BEGIN:
            (name, state) = matchStartLine(line, filename)
            if state != STATE_BEGIN: 
                MapSourceFiles[name] = filename
                RunTests[name] = {}
                RunTests[name][TYPE] = state
                RunTests[name][TESTLINES] = []

            if state == STATE_ARANGOSH_RUN:
                RunTests[name][LINE_NO] = lineNo;
                RunTests[name][STRING] = "";
            continue

        # we are within a example
        line = line[len(strip):]
        showCmd = True
        
        # end-example test
        m = endExample.match(line)

        if m:
            name = ""
            partialLine = ""
            partialCmd = ""
            state = STATE_BEGIN
            continue

        line = line.lstrip('/');
        line = line.lstrip(' ');
        if state == STATE_ARANGOSH_OUTPUT:
            line = line.replace("\\", "\\\\").replace("'", "\\'")
        #print line
        # handle any continued line magic
        if line != "":
            if line[0] == "|":
                if line.startswith("| "):
                    line = line[2:]
                elif line.startswith("|~ "):
                    showCmd = False
                    line = line[3:]
                elif line.startswith("|~"):
                    showCmd = False
                    line = line[2:]
                else:
                    line = line[1:]

                if state == STATE_ARANGOSH_OUTPUT:
                    partialLine = partialLine + line + "\\n"
                else:
                    partialLine = partialLine + line + "\n"
                continue

            if line[0] == "~":
                showCmd = False
                if line.startswith("~ "):
                    line = line[2:]
                else:
                    line = line[1:]

            elif line.startswith("  "):
                line = line[2:]

        line = partialLine + line
        partialLine = ""

        if state == STATE_ARANGOSH_OUTPUT:
            RunTests[name][TESTLINES].append([line, showCmd, lineNo])
        elif state == STATE_ARANGOSH_RUN:
            RunTests[name][STRING] += line + "\n"


def generateSetupFunction():
    print
    print "(function () {\n%s}());" % ArangoshSetup
    print

################################################################################
### @brief generate arangosh example
################################################################################

loopDetectRE = re.compile(r'^[ \n]*(while|if|var|throw|for) ')
expectErrorRE = re.compile(r'.*// *xpError\((.*)\).*')
#expectErrorRE = re.compile(r'.*//\s*xpError\(([^)]*)\)/')

def generateArangoshOutput(testName):
    value = RunTests[testName]
    #print value
    #print value[TESTLINES][0][2]
    #print type(value[TESTLINES][0][2])
    if (len(value[TESTLINES]) == 0) or (len(value[TESTLINES][0]) < 3):
        print >> sys.stderr, "syntax error in %s - its empty! Maybe you've used to many pipes?" %(testName)
        raise Exception
    try:
        print '''
%s
/// %s
(function() {
  countErrors = 0;
  var testName = '%s';
  var startLineCount = %d;
  var lineCount = 0;
  var outputDir = '%s';
  var sourceFile = '%s';
  var startTime = time();
  internal.startCaptureMode();
'''    %   (
        ('/'*80),
        testName,
        testName,
        value[TESTLINES][0][2],
        escapeBS.sub(doubleBS, OutputDir),
        escapeBS.sub(doubleBS, MapSourceFiles[testName])
        )
    except Exception as x:
        print x
        print testName
        print value
        raise

    for l in value[TESTLINES]:
        # try to match for errors, and remove the comment.
        expectError = 'undefined'
        m = expectErrorRE.match(l[0])
        if m:
            expectError = "'" + m.group(1).strip().strip('\\n') + "'"
            l[0] = l[0][0:l[0].find('//')].rstrip(' ')

        m = loopDetectRE.match(l[0])
        fakeVar = 'false'
        if m and l[0][0:3] == 'var':
            count = l[0].find('=')
            print  "  " + l[0][0:count].rstrip(' ') + ";"
            l[0] = l[0][4:]
            fakeVar = 'true'

        print "  runTestLine('%s', testName, sourceFile, %s, lineCount++, %s, %s, %s, %s);" % (
            l[0],                         # the test string
            l[2],                         # line in the source file
            'true' if l[1] else 'false',  # Is it visible in the documentation?
            expectError,                  # will it throw? if the errorcode else undefined.
            'true' if m    else 'false',  # is it a loop construct? (will be evaluated different)
            fakeVar                       # 'var ' should be printed
            )
    print '''  var output = internal.stopCaptureMode();

  print("[" + (time () - startTime) + "s] done with  " + testName);
  output = highlight("js", output);
  fs.write(outputDir + fs.pathSeparator + testName + '.generated', output);
  checkForOrphanTestCollections('not all collections were cleaned up after ' + sourceFile + ' Line[' + startLineCount + '] [' + testName + ']:');
}());
'''

################################################################################
### @brief generate arangosh run
################################################################################

def generateArangoshRun(testName):

    if JS_DEBUG:
        print "internal.output('%s\\n');" % ('=' * 80)
        print "internal.output('ARANGOSH RUN\\n');"
        print "internal.output('%s\\n');" % ('=' * 80)

    value = RunTests[testName]
    startLineNo = RunTests[testName][LINE_NO]
    print '''
%s
/// %s
(function() {
  var ArangoshRun = {};
  internal.startPrettyPrint(true);
  internal.stopColorPrint(true);
  var testName = '%s';
  var lineCount = 0;
  var startLineCount = %d;
  var outputDir = '%s';
  var sourceFile = '%s';
  var startTime = time();
  output = '';
  var assert = function(a) { globalAssert(a, testName, sourceFile); };
  testFunc = function() {
%s};
''' %  (
        ('/'*80),
        testName,
        testName,
        startLineNo,
        escapeBS.sub(doubleBS, OutputDir),
        escapeBS.sub(doubleBS, MapSourceFiles[testName]),
        value[STRING].lstrip().rstrip())

    if testName in ArangoshExpect:
        print "  rc = runTestFuncCatch(testFunc, testName, errors.%s);" % (ArangoshExpect[key])
    else:
        print "  rc = runTestFunc(testFunc, testName, sourceFile);"

    print '''
  if (rc === undefined || rc === '' ) {
    rc = " FAILED in " + testName;
  }
  print("[" + (time () - startTime) + "s] " + rc);
  ///output = highlight("js", output);
  fs.write(outputDir + fs.pathSeparator + testName + '.generated', output);
  checkForOrphanTestCollections('not all collections were cleaned up after ' + sourceFile + ' Line[' + startLineCount + '] [' + testName + ']:');
}());
'''

################################################################################
### @brief generate arangosh run
################################################################################

def generateArangoshShutdown():
    print '''
if (allErrors.length > 0) {
    print(allErrors);
    throw new Error('trouble during generating documentation data; see above.');
}
'''

################################################################################
### @brief get file names
################################################################################

def loopDirectories():
    global ArangoshSetup, OutputDir, FilterForTestcase

    argv = sys.argv
    argv.pop(0)
    filenames = []
    fstate = OPTION_NORMAL
    
    for filename in argv:
        if filename == "--arangoshSetup":
            fstate = OPTION_ARANGOSH_SETUP
            continue

        if filename == "--onlyThisOne": 
            fstate = OPTION_FILTER
            continue

        if filename == "--outputDir":
            fstate = OPTION_OUTPUT_DIR
            continue

        if filename == "--outputFile":
            fstate = OPTION_OUTPUT_FILE
            continue

        if fstate == OPTION_NORMAL:
            if os.path.isdir(filename):
                for root, dirs, files in os.walk(filename):
                    for file in files:
                        if (file.endswith(".md") or file.endswith(".mdpp") or file.endswith(".js") or file.endswith(".cpp")):
                            filenames.append(os.path.join(root, file))
            else:
                filenames.append(filename)

        elif fstate == OPTION_FILTER:
            fstate = OPTION_NORMAL
            if (len(filename) > 0): 
                FilterForTestcase = re.compile(filename);

        elif fstate == OPTION_ARANGOSH_SETUP:
            fstate = OPTION_NORMAL
            f = open(filename, "r")
    
            for line in f:
                line = line.rstrip('\n')
                ArangoshSetup += line + "\n"
    
            f.close()

        elif fstate == OPTION_OUTPUT_DIR:
            fstate = OPTION_NORMAL
            OutputDir = filename

        elif fstate == OPTION_OUTPUT_FILE:
            fstate = OPTION_NORMAL
            sys.stdout = open(filename, 'w')

    for filename in filenames:
        if (filename.find("#") < 0):
            f = open(filename, "r")
        
            analyzeFile(f, filename)
    
            f.close()
        else:
            print >> sys.stderr, "skipping %s\n" % (filename)


def generateTestCases():
    global TESTLINES, TYPE, LINE_NO, STRING, RunTests
    testNames = RunTests.keys()
    testNames.sort()
    for thisTest in testNames:
        if RunTests[thisTest][TYPE] == STATE_ARANGOSH_OUTPUT:
            generateArangoshOutput(thisTest)
        elif RunTests[thisTest][TYPE] == STATE_ARANGOSH_RUN:
            generateArangoshRun(thisTest)

################################################################################
### @brief main
################################################################################

loopDirectories()
print >> sys.stderr, "filtering test %d cases" %(len(filterTestList))

generateArangoshHeader()
generateSetupFunction()
generateTestCases()

generateArangoshShutdown()
