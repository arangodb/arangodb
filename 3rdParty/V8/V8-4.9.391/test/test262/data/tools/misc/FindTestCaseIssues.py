# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

#--Imports---------------------------------------------------------------------
import argparse
import os
import sys
import re

#--Globals---------------------------------------------------------------------
testCaseRe     = re.compile(r"function\W+testcase\W*\(\W*\)")
runTestCaseRe  = re.compile(r"runTestCase\W*\(\W*testcase\W*\)")

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
def handleFile(filePath):
    with open(filePath, "r") as f:
        origLines = f.readlines()

    testCase = False
    runTestCaseCalled = False
    for line in origLines:
        if testCaseRe.search(line)!=None:
            testCase = True
        if runTestCaseRe.search(line)!=None:
            runTestCaseCalled = True

    if testCase==True and runTestCaseCalled==True:
        pass #print "testcase TEST:", filePath
    elif testCase==False and runTestCaseCalled==False:
        pass #print "GLOBAL TEST:", filePath
    else:
        print "ERROR:", filePath



#--Main------------------------------------------------------------------------
if __name__=="__main__":
    __parser = argparse.ArgumentParser(description='Tool used to detect (potentially) invalid test cases')
    __parser.add_argument('tpath', action='store',
                          help='Full path to test cases. E.g., C:\repos\test262-msft\test\suite')
    ARGS = __parser.parse_args()
    if not os.path.exists(ARGS.tpath):
        print "Cannot examine tests in '%s' when it doesn't exist!" % ARGS.tpath
        sys.exit(1)

    ALL_JS_FILES = getAllJSFiles(ARGS.tpath)
    for fileName in ALL_JS_FILES:
        handleFile(fileName)
    print "Done!"
