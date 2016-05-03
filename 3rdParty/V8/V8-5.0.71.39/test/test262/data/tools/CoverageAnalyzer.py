# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.


#--IMPORTS---------------------------------------------------------------------
import os
import sys

#--GLOBALS---------------------------------------------------------------------
CVG_DICT = {}

#--HELPERS---------------------------------------------------------------------
def getCoverageData(directory):
  tempList = os.listdir(directory)
  #Build up a list of directories under directory
  dirList = [x for x in tempList if os.path.isdir(os.path.join(directory, x))]
  #Build up a list of JavaScript files under the current directory
  jsList = [x for xin in tempList if x.endswith(".js")]

  #If the directory contains JavaScript files we'll assume they're all test
  #cases
  if len(jsList)!=0:
    CVG_DICT[os.path.split(directory)[1]] = len(jsList)

  #This might have just been a directory containing other dirs. Call ourself on
  #it as well
  for x in dirList:
    getCoverageData(os.path.join(directory, x))


def emitCoverageData(cvgDict):
  totalTests = 0
  totalSections = 0
  keyList = cvgDict.keys()
  keyList.sort(chapterCompare)
  for cvgKey in keyList:
    print cvgKey, ",", cvgDict[cvgKey]
    totalSections+=1
    totalTests+=cvgDict[cvgKey]
  print
  print "Total number of tests is:", totalTests, "."
  print "These tests cover", totalSections, "ECMAScript 5 sections."


def chapterCompare(x, y):
  if ("." in x) and ("." in y):
    try:
      x1 = int(x[0:x.index(".")])
      y1 = int(y[0:y.index(".")])
      if x1==y1:
        return chapterCompare(x[x.index(".")+1:], y[y.index(".")+1:])
      return cmp(x1, y1)
    except ValueError:
      pass
  return cmp(x, y)

#--MAIN------------------------------------------------------------------------
startDir = sys.argv[1]
getCoverageData(startDir)
print "Emitting ECMAScript 5 coverage data for", startDir, "..."
emitCoverageData(CVG_DICT)
