# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

#--Imports---------------------------------------------------------------------
import argparse
import os
import sys
import re

#--Globals---------------------------------------------------------------------

#List of regular expressions covering suspect code snippets which might be
#invalid from an ES5 POV
QUESTIONABLE_RE_LIST = ["window",
                        "document(?!ation)",
                        "alert",
                        "setTimeout",
                        "ActiveX",
                        ]
QUESTIONABLE_RE_LIST = [re.compile(x, re.I) for x in QUESTIONABLE_RE_LIST]

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

    for line in origLines:
        for tempRe in QUESTIONABLE_RE_LIST:
            if tempRe.search(line)!=None:
                print filePath
                print "\t", line

#--Main------------------------------------------------------------------------
if __name__=="__main__":
    __parser = argparse.ArgumentParser(description='Tool used to detect (potentially) invalid test cases')
    __parser.add_argument('tpath', action='store',
                          help='Full path to test cases. E.g., C:\repos\test262-msft\test\suite\ietestcenter')
    ARGS = __parser.parse_args()
    if not os.path.exists(ARGS.tpath):
        print "Cannot examine tests in '%s' when it doesn't exist!" % ARGS.tpath
        sys.exit(1)

    ALL_JS_FILES = getAllJSFiles(ARGS.tpath)
    for fileName in ALL_JS_FILES:
        handleFile(fileName)
    print "Done!"
