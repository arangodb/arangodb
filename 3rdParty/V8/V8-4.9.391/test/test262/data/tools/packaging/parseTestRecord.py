#!/usr/bin/env python

# Copyright 2011 by Google, Inc.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

# TODO: resolve differences with common.py and unify into one file.


import logging
import optparse
import os
from os import path
import platform
import re
import subprocess
import sys
import tempfile
import time
import imp

# from TestCasePackagerConfig import *

headerPatternStr = r"(?:(?:\s*\/\/.*)?\s*\n)*"
captureCommentPatternStr = r"\/\*\*?((?:\s|\S)*?)\*\/\s*\n"
anyPatternStr = r"(?:\s|\S)*"

headerPattern = re.compile("^" + headerPatternStr)

# Should match anything
testRecordPattern = re.compile(r"^(" + headerPatternStr +
                               r")(?:" + captureCommentPatternStr +
                               r")?(" + anyPatternStr +
                               r")$")

stars = re.compile(r"\s*\n\s*\*\s?")
atattrs = re.compile(r"\s*\n\s*\*\s*@")

yamlPattern = re.compile(r"---((?:\s|\S)*)---")
newlinePattern = re.compile(r"\n")

yamlLoad = None

def stripStars(text):
    return stars.sub('\n', text).strip()

def stripHeader(src):
    header = headerPattern.match(src).group(0)
    return src[len(header):]

def matchParts(src, name):
    match = testRecordPattern.match(src)
    if match == None:
        raise Exception('unrecognized: ' + name)
    return match

def hasYAML(text):
    match = yamlPattern.match(text)
    if match == None:
        return False
    return True

def oldAttrParser(testRecord, body, name):
    propTexts = atattrs.split(body)
    testRecord['commentary'] = stripStars(propTexts[0])
    del propTexts[0]
    for propText in propTexts:
        propMatch = re.match(r"^\w+", propText)
        if propMatch == None:
            raise Exception('Malformed "@" attribute: ' + name)
        propName = propMatch.group(0)
        propVal = stripStars(propText[len(propName):])

        if propName in testRecord:
            raise Exception('duplicate: ' + propName)
        testRecord[propName] = propVal;

def yamlAttrParser(testRecord, attrs, name):
    match = yamlPattern.match(attrs)
    body = match.group(1)
    importYamlLoad()
    parsed = yamlLoad(body)

    if (parsed is None):
        print "Failed to parse yaml in name %s"%(name)
        return

    for key in parsed:
        value = parsed[key]
        if key == "info":
            key = "commentary"
        testRecord[key] = value

    if 'flags' in testRecord:
        for flag in testRecord['flags']:
            testRecord[flag] = ""

def parseTestRecord(src, name):
    testRecord = {}
    match = matchParts(src, name)
    testRecord['header'] = match.group(1).strip()
    testRecord['test'] = match.group(3) # do not trim

    attrs = match.group(2)
    if attrs:
        if hasYAML(attrs):
            yamlAttrParser(testRecord, attrs, name)
        else:
            oldAttrParser(testRecord, attrs, name)

    return testRecord

def importYamlLoad():
    global yamlLoad
    if yamlLoad:
        return
    monkeyYaml = loadMonkeyYaml()
    yamlLoad = monkeyYaml.load

def loadMonkeyYaml():
    f = None
    try:
        p = os.path.dirname(os.path.realpath(__file__))
        (f, pathname, description) = imp.find_module("monkeyYaml", [p])
        module = imp.load_module("monkeyYaml", f, pathname, description)
        return module
    except:
        raise ImportError("Cannot load monkeyYaml")
    finally:
        if f:
            f.close()
