#!/usr/bin/env python
# Copyright 2014 by Sam Mikes.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

# This code provides a fallback parser that can handle the subset of
# YAML used in test262 frontmatter

import re

mYamlKV = re.compile(r"(.*?):(.*)")
mYamlIntDigits = re.compile(r"^[-0-9]*$")
mYamlFloatDigits = re.compile(r"^[-.0-9eE]*$")
mYamlListPattern = re.compile(r"^\[(.*)\]$")
mYamlMultilineList = re.compile(r"^ *- (.*)$")

def load(str):
    dict = None
    key = None
    emptyLines = 0

    lines = str.splitlines()
    while lines:
        line = lines.pop(0)
        if myIsAllSpaces(line):
            emptyLines += 1
            continue
        result = mYamlKV.match(line)

        if result:
            if not dict:
                dict = {}
            key = result.group(1).strip()
            value = result.group(2).strip()
            (lines, value) = myReadValue(lines, value)
            dict[key] = value
        else:
            if dict and key and key in dict:
                c = " " if emptyLines == 0 else "\n" * emptyLines
                dict[key] += c + line.strip()
            else:
                raise Exception("monkeyYaml is confused at " + line)
        emptyLines = 0
    return dict

def myReadValue(lines, value):
    if value == ">":
        (lines, value) = myMultiline(lines, value)
        value = value + "\n"
        return (lines, value)
    if lines and not value and myMaybeList(lines[0]):
        return myMultilineList(lines, value)
    else:
        return lines, myReadOneLine(value)

def myMaybeList(value):
    return mYamlMultilineList.match(value)

def myMultilineList(lines, value):
    # assume no explcit indentor (otherwise have to parse value)
    value = []
    indent = None
    while lines:
        line = lines.pop(0)
        leading = myLeadingSpaces(line)
        if myIsAllSpaces(line):
            pass
        elif leading < indent:
            lines.insert(0, line)
            break;
        else:
            indent = indent or leading
            value += [myReadOneLine(myRemoveListHeader(indent, line))]
    return (lines, value)

def myRemoveListHeader(indent, line):
    line = line[indent:]
    return mYamlMultilineList.match(line).group(1)

def myReadOneLine(value):
    if mYamlListPattern.match(value):
        return myFlowList(value)
    elif mYamlIntDigits.match(value):
        try:
            value = int(value)
        except ValueError:
            pass
    elif mYamlFloatDigits.match(value):
        try:
            value = float(value)
        except ValueError:
            pass
    return value

def myFlowList(value):
    result = mYamlListPattern.match(value)
    values = result.group(1).split(",")
    return [myReadOneLine(v.strip()) for v in values]

def myMultiline(lines, value):
    # assume no explcit indentor (otherwise have to parse value)
    value = []
    indent = myLeadingSpaces(lines[0])
    while lines:
        line = lines.pop(0)
        if myIsAllSpaces(line):
            value += ["\n"]
        elif myLeadingSpaces(line) < indent:
            lines.insert(0, line)
            break;
        else:
            value += [line[(indent):]]
    value = " ".join(value)
    return (lines, value)

def myIsAllSpaces(line):
    return len(line.strip()) == 0

def myLeadingSpaces(line):
    return len(line) - len(line.lstrip(' '))
