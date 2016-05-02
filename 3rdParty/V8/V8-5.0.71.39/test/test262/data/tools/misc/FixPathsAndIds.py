# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

#--Imports---------------------------------------------------------------------
import argparse
import os
import sys
import re

#--Globals---------------------------------------------------------------------
PRE_PATH = ""

#------------------------------------------------------------------------------
def getAllJSFiles(dirName):
    '''
    Returns all JS files under dirName
    '''
    retVal = []
    if os.path.isfile(dirName) and dirName.endswith(".js"):
        retVal = [dirName]
    elif os.path.isdir(dirName):
        tempList = [os.path.join(dirName, x) for x in os.listdir(dirName)]
        for x in tempList:
            retVal += getAllJSFiles(x)
    #else:
    #    raise Exception("getAllJSFiles: encountered a non-file/non-dir:" + dirName)
    return retVal

#------------------------------------------------------------------------------
def handleFile(filePath, partialPath):
    global PRE_PATH
    tempPath = filePath.replace(partialPath + os.path.sep, "", 1)
    tempPath = tempPath.replace(os.path.sep, "/")

    with open(filePath, "rb") as f:
        origLines = f.readlines()

    with open(filePath, "wb") as f:
        pathHit = False
        #testHit = False
        #descriptHit = False

        for line in origLines:
            #TODO?
            #if (not testHit) and re.match("^$", line)!=None:
            #    #Throw away empty lines until we hit the first test function
            #    continue
            #elif (not testHit) and re.search("test\s*:\s*function\s+testcase\(\)", line)!=None:
            #    testHit = True
            #    line = line.rstrip() + os.linesep
            if (not pathHit) and re.search(r"\* @path\s[^$]", line)!=None:
                lineEnding = "\n"
                if line.endswith("\r\n"):
                    lineEnding = "\r\n"
                pathHit = True
                line = re.sub(r"@path\s+[^$]+$", #"\"[^\"]*\"",
                              r"@path %s%s" % (PRE_PATH + tempPath, lineEnding),
                              line)
            #TODO?
            #elif (not descriptHit) and re.search("description\s*:\s*\"", line)!=None:
            #    descriptHit = True
            #    line = line.strip() + os.linesep
            f.write(line)

#--Main------------------------------------------------------------------------
if __name__=="__main__":
    __parser = argparse.ArgumentParser(description='Tool used to fix the path properties of test case objects')
    __parser.add_argument('tpath', action='store',
                          help='Full path to test cases. E.g., C:\repos\test262-msft\test\suite\ietestcenter')
    ARGS = __parser.parse_args()
    if not os.path.exists(ARGS.tpath):
        print "Cannot fix tests in '%s' when it doesn't exist!" % ARGS.tpath
        sys.exit(1)

    ALL_JS_FILES = getAllJSFiles(ARGS.tpath)
    for fileName in ALL_JS_FILES:
        handleFile(fileName, ARGS.tpath)
    print "Done!"
