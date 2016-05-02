# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

#--Imports---------------------------------------------------------------------
import argparse
import os
import sys
import re

#--Globals---------------------------------------------------------------------
PRE_PATH = "TestCases/"

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
    tempId   = tempPath.rsplit("/", 1)[1][:-3]

    with open(filePath, "r") as f:
        origLines = f.readlines()

    with open(filePath, "w") as f:
        pathHit = False
        idHit = False
        testHit = False
        descriptHit = False

        for line in origLines:
            if (not testHit) and re.match("^$", line)!=None:
                #Throw away empty lines until we hit the first test function
                continue
            elif (not testHit) and re.search("test\s*:\s*function\s+testcase\(\)", line)!=None:
                testHit = True
                line = line.rstrip() + os.linesep
            elif (not pathHit) and re.search("path\s*:\s*\"", line)!=None:
                pathHit = True
                line = "path: \"%s\",%s" % (PRE_PATH + tempPath, os.linesep)
            elif (not idHit) and re.search("id\s*:\s*\"", line)!=None:
                idHit = True
                line = "id: \"%s\",%s" % (tempId, os.linesep)
            elif (not descriptHit) and re.search("description\s*:\s*\"", line)!=None:
                descriptHit = True
                line = line.strip() + os.linesep
            f.write(line)

def getPartialPath(tc):
    tc = os.path.splitext(os.path.basename(tc))[0]
    if not ("-" in tc):
        print "'-' not detected in '%s'; cannot continue!" % tc
        sys.exit(1)
    elif not ("." in tc):
        tc = tc.replace("-", ".0-", 1)

    #Generate the partial path of the test case
    tempList = tc.split("-",1)[0].split(".")
    partialPath = ""
    for i in xrange(1, len(tempList)+1):
        partialPath += ".".join(tempList[0:i]) + os.path.sep
    partialPath = os.path.join(partialPath, tc + ".js")
    if partialPath.index(os.path.sep)==1:
        partialPath = "chapter0" + partialPath
    elif partialPath.index(os.path.sep)==2:
        partialPath = "chapter" + partialPath
    return partialPath


#--Main------------------------------------------------------------------------
if __name__=="__main__":
    __parser = argparse.ArgumentParser(description='Tool used to fix the id and path properties of test case objects')
    __parser.add_argument('path', action='store',
                          help='Full path to test cases. E.g., C:\repos\test262-msft\test\suite\ietestcenter')
    __parser.add_argument('add', action='store',
                          help='Command used to add a test file to source control')
    __parser.add_argument('del', action='store',
                          help='Command used to remove a test file from source control')
    __parser.add_argument('tc', action='store',
                          help='test case to move')

    ARGS = __parser.parse_args()
    if not os.path.exists(ARGS.path):
        print "Cannot fix tests in '%s' when it doesn't exist!" % ARGS.path
        sys.exit(1)
    elif not os.path.isfile(ARGS.tc):
        print "Cannot move '%s' when it doesn't exist!" % ARGS.tc

    partialPath = getPartialPath(ARGS.tc)


    print "Done!", partialPath
