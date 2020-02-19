#!/usr/bin/python
# -*- coding: utf-8 -*-
################################################################################
### @brief creates swagger json files from doc headers of rest files
###
### find files in
###   arangod/RestHandler/*.cpp
###   js/actions/api-*.js
###
### @usage generateSwagger.py < RestXXXX.cpp > restSwagger.json
###
### @file
###
### DISCLAIMER
###
### Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
###
### Licensed under the Apache License, Version 2.0 (the "License");
### you may not use this file except in compliance with the License.
### You may obtain a copy of the License at
###
###     http://www.apache.org/licenses/LICENSE-2.0
###
### Unless required by applicable law or agreed to in writing, software
### distributed under the License is distributed on an "AS IS" BASIS,
### WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
### See the License for the specific language governing permissions and
### limitations under the License.
###
### Copyright holder is triAGENS GmbH, Cologne, Germany
###
### @author Dr. Frank Celler
### @author Thomas Richter
### @author Copyright 2014, triAGENS GmbH, Cologne, Germany
################################################################################

from __future__ import print_function # py2 compat
from __future__ import unicode_literals
import sys
import re
import json
import operator
import os
import os.path
import io
#string,
#from pygments import highlight
#from pygments.lexers import YamlLexer
#from pygments.formatters import TerminalFormatter

#, yaml
#import ruamel.yaml as yaml
rc = re.compile
MS = re.M | re.S

################################################################################
### @brief swagger
################################################################################

swagger = {
    "swagger": "2.0",
    "info": {
        "description": "ArangoDB REST API Interface",
        "version": "1.0", # placeholder
        "title": "ArangoDB",
        "license": {
            "name": "Apache License, Version 2.0"
        }
    },
    "basePath": "/",
    "definitions": {
        "ARANGO_ERROR": {
            "description": "An ArangoDB Error code",
            "type": "integer"
        },
        "ArangoError": {
            "description": "the arangodb error type",
            "properties": {
                "code": {
                    "description": "the HTTP Status code",
                    "type": "integer"
                },
                "error": {
                    "description": "boolean flag to indicate whether an error occurred (*true* in this case)",
                    "type": "boolean"
                },
                "errorMessage": {
                    "description": "a descriptive error message describing what happened, may contain additional information",
                    "type": "string"
                },
                "errorNum": {
                    "description": "the ARANGO_ERROR code",
                    "type": "integer"
                }
            }
        }
        
    },
    "paths" : {}
}

################################################################################
### @brief native swagger types
################################################################################

swaggerBaseTypes = [
    'object',
    'array',
    'number',
    'integer',
    'long',
    'float',
    'double',
    'string',
    'byte',
    'binary',
    'boolean',
    'date',
    'dateTime',
    'password'
]

swaggerFormats = {
    "number": ["", "float", "double"],
    "integer": ["", "int32", "int64"]
}

################################################################################
### @brief length of the swagger definition namespace
################################################################################

defLen = len('#/definitions/')

################################################################################
### @brief operation
################################################################################

httpPath = ''

################################################################################
### @brief operation
################################################################################

method = ''

################################################################################
### @brief operation
################################################################################

operation = {}

################################################################################
### @brief current filename
################################################################################

fn = ''

################################################################################
### @brief current section
################################################################################

currentTag = ''

################################################################################
### @brief current docublock
################################################################################

currentDocuBlock = None
lastDocuBlock = None

################################################################################
### @brief index of example block we're reading
################################################################################

currentExample = 0

################################################################################
### @brief the current returncode we're working on
################################################################################

currentReturnCode = 0

################################################################################
### @brief collect json body parameter definitions:
################################################################################

restBodyParam = None
restReplyBodyParam = None
restSubBodyParam = None


operationIDs = []

################################################################################
### @brief DEBUG
################################################################################

DEBUG = True
DEBUG = False

################################################################################
### @brief facility to remove leading and trailing html-linebreaks
################################################################################
removeTrailingBR = re.compile("<br>$")
removeLeadingBR = re.compile("^<br>")

def brTrim(text):
    return removeLeadingBR.sub("", removeTrailingBR.sub("", text.strip(' ')))

################################################################################
### @brief check for token to be right
################################################################################

reqOpt = ["required", "optional"]
def CheckReqOpt(token):
    if token not in reqOpt:
        print("This is supposed to be required or optional!", file=sys.stderr)
        raise Exception("invalid value '%s'" % token)

################################################################################
### @brief trim_text
################################################################################

def trim_text(txt):
    r = rc(r"""[ \t]+$""")
    txt = r.sub("", txt)

    return txt

################################################################################
### @brief parameters
###
### suche die erste {
### suche die letzten }
### gib alles dazwischen zurck
################################################################################

def parameters(line):
    (_l, _c, line) = line.partition('{')
    (line, _c, _r) = line.rpartition('}')
    line = BackTicks(line, wordboundary=['{', '}'])

    return line

################################################################################
### @brief BackTicks
###
### `word` -> <b>word</b>
################################################################################

def BackTicks(txt, wordboundary=['<em>', '</em>']):
    r = rc(r"""([\(\s'/">]|^|.)\`(.*?)\`([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief AsteriskItalic
###
### *word* -> <b>word</b>
################################################################################

def AsteriskItalic(txt, wordboundary=['<strong>', '</strong>']):
    r = rc(r"""([\(\s'/">]|^|.)\*(.*?)\*([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief AsteriskBold
###
### **word** -> <b>word</b>
################################################################################

def AsteriskBold(txt, wordboundary=['<strong>', '</strong>']):
    r = rc(r"""([\(\s'/">]|^|.)\*\*(.*?)\*\*([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief FA
###
### @FA{word} -> <b>word</b>
################################################################################

def FA(txt, wordboundary=['<b>', '</b>']):
    r = rc(r"""([\(\s'/">]|^|.)@FA\{(.*?)\}([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief FN
###
### @FN{word} -> <b>word</b>
################################################################################

def FN(txt, wordboundary=['<b>', '</b>']):
    r = rc(r"""([\(\s'/">]|^|.)@FN\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief LIT
###
### @LIT{word} -> <b>word</b>
################################################################################

def LIT(txt, wordboundary=['<b>', '</b>']):
    r = rc(r"""([\(\s'/">]|^)@LIT\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief LIT
###
### \ -> needs to become \\ so \n's in the text can be differciated.
################################################################################

def BACKSLASH(txt):
    return txt.replace('\\', '\\\\\\')

################################################################################
### @brief Typegraphy
################################################################################

def Typography(txt):
    txt = txt[0:-1]

#    txt = BackTicks(txt)
#    txt = AsteriskBold(txt)
#    txt = AsteriskItalic(txt)
#    txt = FN(txt)
#    txt = LIT(txt)
#    txt = FA(txt)
#
# no way to find out the correct link for Swagger,
# so replace all @ref elements with just "the manual"

    r = rc(r"""@ref [a-zA-Z0-9]+""", MS)
    txt = r.sub("the manual", txt)
    txt = re.sub(r"@endDocuBlock", "", txt)
    txt = BACKSLASH(txt)
    return txt

################################################################################
### @brief InitializationError
################################################################################

class InitializationError(Exception):
    pass

################################################################################
### @brief StateMachine
################################################################################

class StateMachine:
    def __init__(self):
        self.handlers = []
        self.startState = None
        self.endStates = []
        self.file = ''
        self.fn = ''

    def add_state(self, handler, end_state=0):
        self.handlers.append(handler)

        if end_state:
            self.endStates.append(handler)

    def set_fn(self, filename):
        self.fn = filename
        global fn
        fn = filename

    def set_start(self, handler):
        self.startState = handler

    def run(self, cargo=None):
        if not self.startState:
            raise InitializationError("must call .set_start() before .run()")

        if not self.endStates:
            raise InitializationError("at least one state must be an end_state")

        handler = self.startState
        try:
            while 1:
                (newState, cargo) = handler(cargo)

                if newState in self.endStates:
                    newState(cargo)
                    break
                elif newState not in self.handlers:
                    raise RuntimeError("Invalid target %s" % newState)
                else:
                    handler = newState
        except Exception as x:
            print("while parsing '" + self.fn + "'", file=sys.stderr)
            print("trying to use handler '"  + handler.__name__ + "'", file=sys.stderr)
            raise x

################################################################################
### @brief Regexen
################################################################################

class Regexen:
    def __init__(self):
        self.DESCRIPTION_LI = re.compile(r'^-\s.*$')
        self.DESCRIPTION_SP = re.compile(r'^\s\s.*$')
        self.DESCRIPTION_BL = re.compile(r'^\s*$')
        self.EMPTY_LINE = re.compile(r'^\s*$')
        self.START_DOCUBLOCK = re.compile('.*@startDocuBlock ')
        self.HINTS = re.compile('.*@HINTS')
        self.END_EXAMPLE_ARANGOSH_RUN = re.compile('.*@END_EXAMPLE_ARANGOSH_RUN')
        self.EXAMPLES = re.compile('.*@EXAMPLES')
        self.EXAMPLE_ARANGOSH_RUN = re.compile('.*@EXAMPLE_ARANGOSH_RUN{')
        self.RESTBODYPARAM = re.compile('.*@RESTBODYPARAM')
        self.RESTSTRUCT = re.compile('.*@RESTSTRUCT')
        self.RESTALLBODYPARAM = re.compile('.*@RESTALLBODYPARAM')
        self.RESTDESCRIPTION = re.compile('.*@RESTDESCRIPTION')
        self.RESTDONE = re.compile('.*@RESTDONE')
        self.RESTHEADER = re.compile('.*@RESTHEADER{')
        self.RESTHEADERPARAM = re.compile('.*@RESTHEADERPARAM{')
        self.RESTHEADERPARAMETERS = re.compile('.*@RESTHEADERPARAMETERS')
        self.RESTQUERYPARAM = re.compile('.*@RESTQUERYPARAM{')
        self.RESTQUERYPARAMETERS = re.compile('.*@RESTQUERYPARAMETERS')
        self.RESTREPLYBODY = re.compile('.*@RESTREPLYBODY')
        self.RESTRETURNCODE = re.compile('.*@RESTRETURNCODE{')
        self.RESTRETURNCODES = re.compile('.*@RESTRETURNCODES')
        self.RESTURLPARAM = re.compile('.*@RESTURLPARAM{')
        self.RESTURLPARAMETERS = re.compile('.*@RESTURLPARAMETERS')
        self.RESTPARAM = re.compile('.*@RESTPARAM')
        self.RESTQUERYPARAMS = re.compile('.*@RESTQUERYPARAMS')
        self.TRIPLENEWLINEATSTART = re.compile(r'^\n\n\n')

################################################################################
### @brief checks for end of comment
################################################################################

def check_end_of_comment(line, r):
    return r.RESTDONE.match(line)

################################################################################
### @brief next_step
################################################################################

def next_step(fp, line, r):
    global operation

    if not line:
        return eof, (fp, line)
    elif check_end_of_comment(line, r):
        return skip_code, (fp, line)
    elif r.START_DOCUBLOCK.match(line):
        return start_docublock, (fp, line)
    elif r.HINTS.match(line):
        return hints, (fp, line)
    elif r.EXAMPLE_ARANGOSH_RUN.match(line):
        return example_arangosh_run, (fp, line)
    elif r.RESTBODYPARAM.match(line):
        return restbodyparam, (fp, line)
    elif r.RESTSTRUCT.match(line):
        return reststruct, (fp, line)
    elif r.RESTALLBODYPARAM.match(line):
        return restallbodyparam, (fp, line)
    elif r.RESTDESCRIPTION.match(line):
        return restdescription, (fp, line)
    elif r.RESTHEADER.match(line):
        return restheader, (fp, line)
    elif r.RESTHEADERPARAM.match(line):
        return restheaderparam, (fp, line)
    elif r.RESTHEADERPARAMETERS.match(line):
        return restheaderparameters, (fp, line)
    elif r.RESTQUERYPARAM.match(line):
        return restqueryparam, (fp, line)
    elif r.RESTQUERYPARAMETERS.match(line):
        return restqueryparameters, (fp, line)
    elif r.RESTREPLYBODY.match(line):
        return restreplybody, (fp, line)
    elif r.RESTRETURNCODE.match(line):
        return restreturncode, (fp, line)
    elif r.RESTRETURNCODES.match(line):
        return restreturncodes, (fp, line)
    elif r.RESTURLPARAM.match(line):
        return resturlparam, (fp, line)
    elif r.RESTURLPARAMETERS.match(line):
        return resturlparameters, (fp, line)
    elif r.RESTPARAM.match(line):
        return restparam, (fp, line)
    elif r.RESTQUERYPARAMS.match(line):
        return restqueryparams, (fp, line)
    elif r.EXAMPLES.match(line):
        return examples, (fp, line)

    return None, None

################################################################################
### @brief generic handler
################################################################################

def generic_handler(cargo, r, message):
    global DEBUG

    if DEBUG:
        print(message, file=sys.stderr)
    (fp, last) = cargo
    last
    while 1:
        (next, c) = next_step(fp, fp.readline(), r)

        if next:
            return next, c

################################################################################
### @brief generic handler with description
### @param cargo the file we're working on
### @param r the regex that matched
### @param message
### @param op
### @param para an object were we should write to
### @param name the key in para we should write to
################################################################################

def generic_handler_desc(cargo, r, message, op, para, name):
    global DEBUG, operation

    if DEBUG:
        print(message, file=sys.stderr)
    (fp, dummy) = cargo

    while 1:
        line = fp.readline()
        (next, c) = next_step(fp, line, r)

        if next:
            para[name] = trim_text(para[name])

            if op:
                try:
                    operation[op].append(para)
                except AttributeError as x:
                    print("trying to set '%s' on operations - failed. '%s'" % (op, para), file=sys.stderr)
                    raise x
            return next, c

        line = Typography(line)
        para[name] += line + '\n'

def start_docublock(cargo, r=Regexen()):
    global currentDocuBlock
    (dummy, last) = cargo
    try:
        # TODO remove when all /// are removed from the docublocks
        if last.startswith('/// '):
            currentDocuBlock = last.split(' ')[2].rstrip()
        else:
            currentDocuBlock = last.split(' ')[1].rstrip()
    except Exception as x:
        print("failed to fetch docublock in '" + last + "': " + str(x), file=sys.stderr)
        raise x

    return generic_handler(cargo, r, 'start_docublock')


def setRequired(where, which):
    if not 'required' in where:
        where['required'] = []
        where['required'].append(which)

################################################################################
### @brief restparam - deprecated - abort.
################################################################################
def restparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, fn, currentExample, currentReturnCode, currentDocuBlock, lastDocuBlock, restReplyBodyParam
    print("deprecated RESTPARAM declaration detected:", file=sys.stderr)
    print(json.dumps(
        swagger['paths'][httpPath],
        indent=4,
        separators=(', ', ': '),
        sort_keys=True), file=sys.stderr)
    raise Exception("RESTPARAM not supported anymore.")

################################################################################
### @brief restparam - deprecated - abort.
################################################################################
def restqueryparams(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, fn, currentExample, currentReturnCode, currentDocuBlock, lastDocuBlock, restReplyBodyParam
    print("deprecated RESTQUERYPARAMS declaration detected:", file=sys.stderr)
    print(json.dumps(
        swagger['paths'][httpPath],
        indent=4,
        separators=(', ', ': '),
        sort_keys=True), file=sys.stderr)
    raise Exception("RESTQUERYPARAMS not supported anymore. Use RESTQUERYPARAMETERS instead.")

################################################################################
### @brief restheader
################################################################################

def restheader(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, fn, currentExample, currentReturnCode, currentDocuBlock, lastDocuBlock, restReplyBodyParam

    currentReturnCode = 0
    currentExample = 0
    restReplyBodyParam = None
    restBodyParam = None

    (dummy, last) = cargo

    temp = parameters(last).split(',')
    if temp == "":
        raise Exception("Invalid restheader value. got empty string. Maybe missing closing bracket? " + last)

    (ucmethod, path) = temp[0].split()

    #TODO: hier checken, ob der letzte alles hatte (responses)
    summary = temp[1]
    summaryList = summary.split()
    method = ucmethod.lower()
    nickname = summaryList[0] + ''.join([word.capitalize() for word in summaryList[1:]])

    httpPath = FA(path, wordboundary=['{', '}'])
    if not httpPath in swagger['paths']:
        swagger['paths'][httpPath] = {}
    if method in swagger['paths'][httpPath]:
        print("duplicate route detected:", file=sys.stderr)
        print("There already is a route [" + ucmethod + " " + httpPath + "]: ", file=sys.stderr)
        print(json.dumps(
            swagger['paths'][httpPath],
            indent=4,
            separators=(', ', ': '),
            sort_keys=True), file=sys.stderr)
        raise Exception("Duplicate route")

    if currentDocuBlock == None:
        raise Exception("No docublock started for this restheader: " + ucmethod + " " + path)

    if lastDocuBlock != None and currentDocuBlock == lastDocuBlock:
        raise Exception("No new docublock started for this restheader: " + ucmethod + " " + path  + ' : ' + currentDocuBlock)

    operationId = nickname
    if len(temp) > 2:
        operationId = temp[2]
    if operationId in operationIDs:
        print(operationIDs)
        raise Exception("duplicate operation ID! " + operationId)
    lastDocuBlock = currentDocuBlock


    swagger['paths'][httpPath][method] = {
        'operationId': operationId.strip(),
        'x-filename': fn,
        'x-hints': '',
        'x-examples': [],
        'tags': [currentTag],
        'summary': summary.strip(),
        'description': '',
        'parameters' : [],
        }
    operation = swagger['paths'][httpPath][method]
    return generic_handler(cargo, r, "resturlparameters")

################################################################################
### @brief resturlparameters
################################################################################

def resturlparameters(cargo, r=Regexen()):
    return generic_handler(cargo, r, "resturlparameters")

################################################################################
### @brief resturlparam
################################################################################

def resturlparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    (dummy, last) = cargo
    name = ""
    pformat = ""
    required = ""

    try:
        (name, pformat, required) = parameters(last).split(',')
    except Exception:
        print("RESTURLPARAM: 3 arguments required. You gave me: " + parameters(last), file=sys.stderr)
        raise x

    if required.strip() != 'required':
        print("only required is supported in RESTURLPARAM", file=sys.stderr)
        raise Exception("invalid url parameter")

    para = {
        'name': name.strip(),
        'in': 'path',
        'format': pformat.strip(),
        'description': '',
        'type': pformat.strip().lower(),
        'required': True
        }
    swagger['paths'][httpPath][method]['parameters'].append(para)

    return generic_handler_desc(cargo, r, "resturlparam", None, para, 'description')

################################################################################
### @brief restqueryparameters
################################################################################

def restqueryparameters(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restqueryparameters")

################################################################################
### @brief restheaderparameters
################################################################################

def restheaderparameters(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restheaderparameters")

################################################################################
### @brief restheaderparameters
################################################################################

def restheaderparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    (dummy, last) = cargo

    parametersList = parameters(last).split(',')

    para = {
        'in': 'header',
        'type': parametersList[1].lower(),
        'name': parametersList[0],
        'description': ''
        }
    swagger['paths'][httpPath][method]['parameters'].append(para)

    return generic_handler_desc(cargo, r, "restheaderparam", None, para, 'description')

################################################################################
### @brief restbodyparam
################################################################################

def restbodyparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, fn, currentDocuBlock
    (dummy, last) = cargo

    try:
        (name, ptype, required, ptype2) = parameters(last).split(',')
    except Exception:
        print("RESTBODYPARAM: 4 arguments required. You gave me: " + parameters(last), file=sys.stderr)
        print("In this docublock: " + currentDocuBlock, file=sys.stderr)
        raise Exception("Argument count error")

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False

    if restBodyParam == None:
        # https://github.com/swagger-api/swagger-ui/issues/1430
        # once this is solved we can skip this:
        operation['description'] += "\n**A JSON object with these properties is required:**\n"
        restBodyParam = {
            'name': 'Json Request Body',
            'x-description-offset': len(swagger['paths'][httpPath][method]['description']),
            'in': 'body',
            'required': True,
            'schema': {
                '$ref': '#/definitions/' + currentDocuBlock
                }
            }
        swagger['paths'][httpPath][method]['parameters'].append(restBodyParam)

    if not currentDocuBlock in swagger['definitions']:
        swagger['definitions'][currentDocuBlock] = {
            'x-filename': fn,
            'type' : 'object',
            'properties': {},
            }

    swagger['definitions'][currentDocuBlock]['properties'][name] = {
        'type': ptype,
        'description': ''
        }

    if ptype == 'object' and len(ptype2) > 0:
        swagger['definitions'][currentDocuBlock]['properties'][name] = {
            '$ref': '#/definitions/' + ptype2
            }

        if not ptype2 in swagger['definitions']:
            swagger['definitions'][ptype2] = {
                'x-filename': fn,
                'type': 'object',
                'properties' : {},
                'description': ''
                }

        if required:
            setRequired(swagger['definitions'][ptype2], name)

        return generic_handler_desc(cargo, r, "restbodyparam", None,
                                    swagger['definitions'][ptype2],
                                    'description')

    if ptype == 'array':
        if ptype2 not in swaggerBaseTypes:
            swagger['definitions'][currentDocuBlock]['properties'][name]['items'] = {
                '$ref': '#/definitions/' + ptype2
            }
        else:
            swagger['definitions'][currentDocuBlock]['properties'][name]['items'] = {
                'type': ptype2
            }
            if ptype2 == 'object':
                swagger['definitions'][currentDocuBlock]['properties'][name]['items']['additionalProperties'] = {}
    elif ptype == 'object':
        swagger['definitions'][currentDocuBlock]['properties'][name]['additionalProperties'] = {}
    elif ptype != 'string':
        if ptype in swaggerFormats and ptype2 not in swaggerFormats[ptype]:
            print("RESTSTRUCT: ptype2 (format)[" + ptype2 + "]  not valid: " + parameters(last), file=sys.stderr)
            raise Exception("'%s' is not one of %s!" % (ptype2, str(swaggerFormats)))
        swagger['definitions'][currentDocuBlock]['properties'][name]['format'] = ptype2


    if required:
        setRequired(swagger['definitions'][currentDocuBlock], name)

    return generic_handler_desc(cargo, r, "restbodyparam", None,
                                swagger['definitions'][currentDocuBlock]['properties'][name],
                                'description')

################################################################################
### @brief restallbodyparam
################################################################################

def restallbodyparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam
    (dummy, last) = cargo

    try:
        (_name, _ptype, required) = parameters(last).split(',')
    except Exception:
        print("RESTALLBODYPARAM: 3 arguments required. You gave me: " + parameters(last), file=sys.stderr)

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False
    if restBodyParam != None:
        raise Exception("May only have one 'ALLBODY'")

    restBodyParam = {
        'name': 'Json Request Body',
        'description': '',
        'in': 'body',
        'x-description-offset': len(swagger['paths'][httpPath][method]['description']),
        'required': required,
        'schema': {
            'type': 'object',
            'additionalProperties': {}
        }
    }
    swagger['paths'][httpPath][method]['parameters'].append(restBodyParam)

    return generic_handler_desc(cargo, r, "restbodyparam", None,
                                restBodyParam,
                                'description')

################################################################################
### @brief reststruct
################################################################################

def reststruct(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, restSubBodyParam, fn
    (dummy, last) = cargo

    try:
        (name, className, ptype, required, ptype2) = parameters(last).split(',')
    except Exception:
        print("RESTSTRUCT: 5 arguments required (name, className, ptype, required, ptype2). You gave me: " + parameters(last), file=sys.stderr)
        raise Exception("Argument count error")

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False

    if className not in swagger['definitions']:
        swagger['definitions'][className] = {
            'type': 'object',
            'properties' : {},
            'description': '',
            'x-filename': fn
            }

    swagger['definitions'][className]['properties'][name] = {
        'type': ptype,
        'description': ''
        }

    if ptype == 'array':
        if ptype2 not in swaggerBaseTypes:
            swagger['definitions'][className]['properties'][name]['items'] = {
                '$ref': '#/definitions/' + ptype2
            }
        else:
            swagger['definitions'][className]['properties'][name]['items'] = {
                'type': ptype2
            }
    if ptype == 'object' and len(ptype2) > 0:
        if not ptype2 in swagger['definitions']:
            swagger['definitions'][ptype2] = {
                'x-filename': fn,
                'type': 'object',
                'properties' : {},
                'description': ''
                }
        swagger['definitions'][className]['properties'][name] = {
            '$ref': '#/definitions/' + ptype2
            }

        if required:
            setRequired(swagger['definitions'][className], name)

        return generic_handler_desc(cargo, r, "reststruct", None,
                                    swagger['definitions'][ptype2],
                                    'description')

    elif ptype != 'string' and ptype != 'boolean':
        if ptype in swaggerFormats and ptype2 not in swaggerFormats[ptype]:
            print("RESTSTRUCT: ptype2 (format)[" + ptype2 + "]  not valid: " + parameters(last), file=sys.stderr)
            raise Exception("'%s' is not one of %s!" % (ptype2, str(swaggerFormats)))
        swagger['definitions'][className]['properties'][name]['format'] = ptype2

    return generic_handler_desc(cargo, r, "restbodyparam", None,
                                swagger['definitions'][className]['properties'][name],
                                'description')

################################################################################
### @brief restqueryparam
################################################################################

def restqueryparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, swaggerBaseTypes
    (dummy, last) = cargo

    parametersList = parameters(last).split(',')

    CheckReqOpt(parametersList[2])
    if parametersList[2] == 'required':
        required = True
    else:
        required = False
    swaggerType = parametersList[1].lower()
    
    if swaggerType not in swaggerBaseTypes:
        print("RESTQUERYPARAM is supposed to be a swagger type.", file=sys.stderr)
        raise Exception("'%s' is not one of %s!" % (swaggerType, str(swaggerBaseTypes)))

    para = {
        'name': parametersList[0],
        'in': 'query',
        'description': '',
        'type': swaggerType,
        'required': required
        }

    swagger['paths'][httpPath][method]['parameters'].append(para)
    return generic_handler_desc(cargo, r, "restqueryparam", None, para, 'description')

################################################################################
### @brief hints
################################################################################

def hints(cargo, r=Regexen()):
    global swagger, operation, httpPath, method

    ret = generic_handler_desc(cargo, r, "hints", None,
                               swagger['paths'][httpPath][method], 'x-hints')

    if r.TRIPLENEWLINEATSTART.match(swagger['paths'][httpPath][method]['x-hints']):
        (fp, dummy) = cargo
        print('remove newline after @HINTS in file %s' % (fp.name), file=sys.stderr)
        exit(1)

    return ret

################################################################################
### @brief restdescription
################################################################################

def restdescription(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    swagger['paths'][httpPath][method]['description'] += '\n\n'

    ret = generic_handler_desc(cargo, r, "restdescription", None,
                               swagger['paths'][httpPath][method],
                               'description')

    if r.TRIPLENEWLINEATSTART.match(swagger['paths'][httpPath][method]['description']):
        (fp, dummy) = cargo
        print('remove newline after @RESTDESCRIPTION in file %s' % (fp.name), file=sys.stderr)
        exit(1)

    return ret

################################################################################
### @brief restreplybody
################################################################################

def restreplybody(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restReplyBodyParam, fn
    (dummy, last) = cargo

    try:
        (name, ptype, required, ptype2) = parameters(last).split(',')
    except Exception:
        print("RESTREPLYBODY: 4 arguments required. You gave me: " + parameters(last), file=sys.stderr)
        raise x

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False

    if currentReturnCode == 0:
        raise Exception("failed to add text to response body: (have to specify the HTTP-code first) " + parameters(last))

    rcBlock = ''
    if name == '':
        if ptype == 'object':
            rcBlock = ptype2
        elif ptype == 'array':
            rcBlock = currentDocuBlock + '_rc_' +  currentReturnCode
    else:
        rcBlock = currentDocuBlock + '_rc_' +  currentReturnCode
    #if currentReturnCode:
    if restReplyBodyParam == None:
        # https://github.com/swagger-api/swagger-ui/issues/1430
        # once this is solved we can skip this:
        operation['description'] += '\n**HTTP ' + currentReturnCode + '**\n'
        operation['description'] += "*A json document with these Properties is returned:*\n"
        operation['responses'][currentReturnCode]['x-description-offset'] = len(operation['description'])

        operation['responses'][currentReturnCode]['schema'] = {
            '$ref': '#/definitions/' + rcBlock
        }
        swagger['paths'][httpPath][method]['produces'] = [
            "application/json"
        ]
        restReplyBodyParam = ''

    if not rcBlock in swagger['definitions']:
        swagger['definitions'][rcBlock] = {
            'x-filename': fn,
            'type' : 'object',
            'properties': {},
        }

    if len(name) > 0:
        swagger['definitions'][rcBlock]['properties'][name] = {
            'type': ptype,
            'description': ''
        }

    if ptype == 'object' and len(ptype2) > 0:
        if len(name) > 0:
            swagger['definitions'][rcBlock]['properties'][name] = {
                '$ref': '#/definitions/' + ptype2
            }

            if not ptype2 in swagger['definitions']:
                swagger['definitions'][ptype2] = {
                    'x-filename': fn,
                    'type': 'object',
                    'properties' : {},
                    'description': ''
                }

                if required:
                    setRequired(swagger['definitions'][ptype2], name)

                return generic_handler_desc(cargo, r, "restbodyparam", None,
                                            swagger['definitions'][ptype2],
                                            'description')

    if ptype == 'array':
        if len(name) == 0:
            swagger['definitions'][rcBlock] = {
                'type': ptype,
                'description': ''
            }
            swagger['definitions'][rcBlock]['items'] = {
                '$ref': '#/definitions/' + ptype2
            }
            return generic_handler_desc(cargo, r, "restreplybody", None,
                                        swagger['definitions'][rcBlock],
                                        'description')
        else:
            if len(ptype2) == 0:
                swagger['definitions'][rcBlock]['properties'][name]['items'] = {
                }
            elif ptype2 not in swaggerBaseTypes:
                swagger['definitions'][rcBlock]['properties'][name]['items'] = {
                    '$ref': '#/definitions/' + ptype2
                }
            else:
                swagger['definitions'][rcBlock]['properties'][name]['items'] = {
                    'type': ptype2
                }
                if ptype2 == 'object':
                    swagger['definitions'][rcBlock]['properties']\
                        [name]['items']['additionalProperties'] = {}
    elif ptype == 'object':
        if len(name) > 0:
            swagger['definitions'][rcBlock]['properties'][name]['additionalProperties'] = {}
    elif ptype != 'string':
        swagger['definitions'][rcBlock]['properties'][name]['format'] = ptype2


    if len(name) > 0 & required:
        setRequired(swagger['definitions'][rcBlock], name)

    if len(name) > 0:
        if 'description' not in swagger['definitions'][rcBlock]['properties']:
            swagger['definitions'][rcBlock]['properties'][name]['description'] = ''

        return generic_handler_desc(
            cargo, r, "restreplybody", None,
            swagger['definitions'][rcBlock]['properties'][name],
            'description')
    else:
        swagger['definitions'][rcBlock]['description'] = ''
        return generic_handler_desc(cargo, r, "restreplybody", None,
                                    swagger['definitions'][rcBlock],
                                    'description')

################################################################################
### @brief restreturncodes
################################################################################

def restreturncodes(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restreturncodes")

################################################################################
### @brief restreturncode
################################################################################

def restreturncode(cargo, r=Regexen()):
    global currentReturnCode, restReplyBodyParam
    (dummy, last) = cargo
    restReplyBodyParam = None
    currentReturnCode = parameters(last)

    if not 'responses' in swagger['paths'][httpPath][method]:
        swagger['paths'][httpPath][method]['responses'] = {}
    swagger['paths'][httpPath][method]['responses'][currentReturnCode] = {
        #'code': parameters(last),
        'description': ''
    }
    return generic_handler_desc(
        cargo, r, "restreturncode", None,
        swagger['paths'][httpPath][method]['responses'][parameters(last)],
        'description')

################################################################################
### @brief examples
################################################################################

def examples(cargo, r=Regexen()):
    global currentExample
    operation['x-examples'].append('')
    return generic_handler_desc(cargo, r, "x-examples", None, operation['x-examples'], currentExample)

################################################################################
### @brief example_arangosh_run
################################################################################


def example_arangosh_run(cargo, r=Regexen()):
    global currentExample, DEBUG

    if DEBUG:
        print("example_arangosh_run", file=sys.stderr)
    (fp, last) = cargo

    exampleHeader = brTrim(operation['x-examples'][currentExample]).strip()

    # new examples code TODO should include for each example own object in json file
    fn = os.path.join(os.path.dirname(__file__), '../Documentation/Examples/' + parameters(last) + '.generated')
    try:
        examplefile = io.open(fn, encoding='utf-8', newline=None)
    except:
        print("Failed to open example file:\n  '%s'" % fn, file=sys.stderr)
        raise Exception("failed to open example file:" + fn)
    operation['x-examples'][currentExample] = '\n\n**Example:**\n ' + exampleHeader.strip('\n ') + '\n\n<pre>'

    for line in examplefile.readlines():
        operation['x-examples'][currentExample] += '<code>' + line + '</code>'
    examplefile.close()

    operation['x-examples'][currentExample] += '</pre>\n\n\n'

    line = ""

    while not r.END_EXAMPLE_ARANGOSH_RUN.match(line):
        line = fp.readline()

        if not line:
            return eof, (fp, line)

    currentExample += 1

    return examples, (fp, line)

################################################################################
### @brief eof
################################################################################

def eof(cargo):
    global DEBUG
    if DEBUG:
        print("eof", file=sys.stderr)

################################################################################
### @brief error
################################################################################

def error(cargo):
    global DEBUG
    if DEBUG:
        print("error", file=sys.stderr)

    sys.stderr.write('Unidentifiable line:\n' + cargo)

################################################################################
### @brief comment
################################################################################

def comment(cargo, r=Regexen()):
    global DEBUG

    if DEBUG:
        print("comment", file=sys.stderr)
    (fp, dummy) = cargo

    while 1:
        line = fp.readline()
        if not line:
            return eof, (fp, line)

        next, c = next_step(fp, line, r)

        if next:
            return next, c

################################################################################
### @brief skip_code
###
### skip all non comment lines
################################################################################

def skip_code(cargo, r=Regexen()):
    global DEBUG

    if DEBUG:
        print("skip_code", file=sys.stderr)
    (fp, last) = cargo

    return comment((fp, last), r)

################################################################################
### @brief main
################################################################################

automat = StateMachine()

automat.add_state(comment)
automat.add_state(eof, end_state=1)
automat.add_state(error, end_state=1)
automat.add_state(start_docublock)
automat.add_state(hints)
automat.add_state(example_arangosh_run)
automat.add_state(examples)
automat.add_state(skip_code)
automat.add_state(restbodyparam)
automat.add_state(reststruct)
automat.add_state(restallbodyparam)
automat.add_state(restdescription)
automat.add_state(restheader)
automat.add_state(restheaderparam)
automat.add_state(restheaderparameters)
automat.add_state(restqueryparam)
automat.add_state(restqueryparameters)
automat.add_state(restreturncode)
automat.add_state(restreturncodes)
automat.add_state(restreplybody)
automat.add_state(resturlparam)
automat.add_state(resturlparameters)
automat.add_state(restparam)
automat.add_state(restqueryparam)


def getOneApi(infile, filename, thisFn):
    automat.set_start(skip_code)
    automat.set_fn(thisFn)
    automat.run((infile, ''))

################################################################################
### Swagger Markdown rendering
################################################################################

def getReference(name, source, verb):
    try:
        ref = name['$ref'][defLen:]
    except Exception:
        print("No reference in: ", file=sys.stderr)
        print(name, file=sys.stderr)
        raise Exception("No reference in: " + name)
    if not ref in swagger['definitions']:
        fn = ''
        if verb:
            fn = swagger['paths'][route][verb]['x-filename']
        else:
            fn = swagger['definitions'][source]['x-filename']
        print(json.dumps(
            swagger['definitions'],
            indent=4,
            separators=(', ', ': '),
            sort_keys=True), file=sys.stderr)
        raise Exception("invalid reference: " + ref + " in " + fn)
    return ref

removeDoubleLF = re.compile("\n\n")
removeLF = re.compile("\n")

def TrimThisParam(text, indent):
    text = text.rstrip('\n').lstrip('\n')
    text = removeDoubleLF.sub("\n", text)
    if indent > 0:
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
            #required = ('required' in swagger['definitions'][reference] and
            #          param in swagger['definitions'][reference]['required'])

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
                lf = ""
                ####
                # print >>sys.stderr, "zz" * layer + param
                if 'type' in thisParam['items']:
                    rc += " (" + thisParam['items']['type']  + ")"
                    lf = "\n"
                else:
                    if len(thisParam['items']) == 0:
                        rc += " (anonymous json object)"
                        lf = "\n"
                    else:
                        trySubStruct = True
                rc += ": " + TrimThisParam(brTrim(thisParam['description']), layer) + lf
                if trySubStruct:
                    try:
                        subStructRef = getReference(thisParam['items'], reference, None)
                    except:
                        print("while analyzing: " + param, file=sys.stderr)
                        print(thisParam, file=sys.stderr)
                    rc += "\n" + unwrapPostJson(subStructRef, layer + 1)
            else:
                if thisParam['type'] not in swaggerDataTypes:
                    print("while analyzing: " + param, file=sys.stderr)
                    print(thisParam['type'] + " is not a valid swagger datatype; supported ones: " + str(swaggerDataTypes), file=sys.stderr)
                    raise Exception("invalid swagger type")
                rc += '  ' * layer + "- **" + param + "**: " + TrimThisParam(thisParam['description'], layer) + '\n'
    return rc




if len(sys.argv) < 4:
    print("usage: " + sys.argv[0] + " <scriptDir> <outDir> <relDir> <docublockdir> <optional: filter>", file=sys.stderr)
    sys.exit(1)

scriptDir = sys.argv[1]
if not scriptDir.endswith("/"):
    scriptDir += "/"

outDir = sys.argv[2]
if not outDir.endswith("/"):
    outDir += "/"

relDir = sys.argv[3]
if not relDir.endswith("/"):
    relDir += "/"

fileFilter = ""
if len(sys.argv) > 5:
    fileFilter = sys.argv[5]
    print("Filtering for: [" + fileFilter + "]", file=sys.stderr)

# read ArangoDB version and use it as API version
f = io.open(scriptDir + "VERSION", encoding="utf-8", newline=None)
version = f.read().strip()
f.close()
swagger['info']['version'] = version

paths = {}

topdir = sys.argv[4]
files = {}

for chapter in os.listdir(topdir):
    if not os.path.isdir(os.path.join(topdir, chapter)) or chapter[0] == ".":
        continue
    files[chapter] = []
    curPath = os.path.join(topdir, chapter)
    for oneFile in os.listdir(curPath):
        if fileFilter != "" and oneFile != fileFilter:
            print("Skipping: [" + oneFile + "]", file=sys.stderr)
            continue
        curPath2 = os.path.join(curPath, oneFile)
        if os.path.isfile(curPath2) and oneFile[0] != "." and oneFile.endswith(".md"):
            files[chapter].append(os.path.join(topdir, chapter, oneFile))

for name, filenames in sorted(files.items(), key=operator.itemgetter(0)):
    currentTag = name
    for fn in filenames:
        thisfn = fn
        infile = io.open(fn, encoding='utf-8', newline=None)
        try:
            getOneApi(infile, name + " - " + ', '.join(filenames), fn)
        except Exception as x:
            print("\nwhile parsing file: '%s' error: %s" % (thisfn, x), file=sys.stderr)
            raise Exception("while parsing file '%s' error: %s" %(thisfn, x))
        infile.close()
        currentDocuBlock = None
        lastDocuBlock = None

# Sort arrays by offset helper:
def descOffsetGet(value):
    return value["descOffset"]

for route in swagger['paths'].keys():
    for verb in swagger['paths'][route].keys():
        offsetPlus = 0
        thisVerb = swagger['paths'][route][verb]
        if not thisVerb['description']:
            print("Description of Route empty; @RESTDESCRIPTION missing?", file=sys.stderr)
            print("in :" + verb + " " + route, file=sys.stderr)
            #raise TODO
        # insert the post json description into the place we extracted it:
        # Collect the blocks we want to work on, sort them by replacement place:
        sortVec = []
        for nParam in range(0, len(thisVerb['parameters'])):
            if thisVerb['parameters'][nParam]['in'] == 'body':
                sortVec.append({
                    "nParam": nParam,
                    "descOffset": thisVerb['parameters'][nParam]['x-description-offset']
                })

        sortVec.sort(key=descOffsetGet)
        for oneItem in sortVec:
            nParam = oneItem["nParam"]
            descOffset = thisVerb['parameters'][nParam]['x-description-offset']
            addText = ''
            postText = ''
            paramDesc = thisVerb['description'][:(descOffset+offsetPlus)]
            if paramDesc:
                postText += paramDesc
            if 'additionalProperties' not in thisVerb['parameters'][nParam]['schema']:
                addText = "\n" + unwrapPostJson(
                    getReference(thisVerb['parameters'][nParam]['schema'],
                                 route,
                                 verb),
                    1) + "\n\n"

            postText += addText
            postText += thisVerb['description'][(offsetPlus+descOffset):]
            offsetPlus += len(addText)
            thisVerb['description'] = postText

        # insert the reply json description into the place we extracted it:
        if 'responses' in thisVerb:

            # Collect the blocks we want to work on, sort them by replacement place:
            sortVec = []
            for nRC in thisVerb['responses']:
                if 'x-description-offset' in thisVerb['responses'][nRC]:
                    sortVec.append({
                        "nParam": nRC,
                        "descOffset": thisVerb['responses'][nRC]['x-description-offset']
                    })

            sortVec.sort(key=descOffsetGet)
            for oneItem in sortVec:
                nRC = oneItem["nParam"]
                descOffset = thisVerb['responses'][nRC]['x-description-offset']
                #print descOffset
                #print offsetPlus
                descOffset += offsetPlus
                addText = ''
                #print thisVerb['responses'][nRC]['description']
                postText = thisVerb['description'][:descOffset]
                #print postText
                replyDescription = TrimThisParam(thisVerb['responses'][nRC]['description'], 0)
                if replyDescription:
                    addText += '\n' + replyDescription + '\n'
                if 'additionalProperties' not in thisVerb['responses'][nRC]['schema']:
                    addText += "\n" + unwrapPostJson(
                        getReference(thisVerb['responses'][nRC]['schema'],
                                     route,
                                     verb),
                        0) + '\n'
                # print addText
                postText += addText
                postText += thisVerb['description'][descOffset:]
                offsetPlus += len(addText)
                thisVerb['description'] = postText

            #print '-'*80
            #print thisVerb['description']

        # Simplify hint box code to something that works in Swagger UI
        # Append the result to the description field
        # Place invisible markers, so that hints can be removed again
        if 'x-hints' in thisVerb and thisVerb['x-hints']:
            thisVerb['description'] += '\n<!-- Hints Start -->'
            tmp = re.sub("{% hint '([^']+?)' %}",
                         lambda match: "\n\n**{}:**  ".format(match.group(1).title()),
                         thisVerb['x-hints'])
            tmp = re.sub('{%[^%]*?%}', '', tmp)
            thisVerb['description'] += tmp
            thisVerb['description'] += '\n<!-- Hints End -->'

        # Append the examples to the description:
        if 'x-examples' in thisVerb and thisVerb['x-examples']:
            thisVerb['description'] += '\n'
            for nExample in range(0, len(thisVerb['x-examples'])):
                thisVerb['description'] += thisVerb['x-examples'][nExample]
            thisVerb['x-examples'] = []# todo unset!

#print highlight(yaml.dump(swagger, Dumper=yaml.RoundTripDumper), YamlLexer(), TerminalFormatter())
#print yaml.dump(swagger, Dumper=yaml.RoundTripDumper)
print(json.dumps(swagger, indent=4, separators=(', ', ': '), sort_keys=True))
#print json.dumps(swagger['paths'], indent=4, separators=(', ',': '), sort_keys=True)
#print highlight(yaml.dump(swagger, Dumper=yaml.RoundTripDumper), YamlLexer(), TerminalFormatter())

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
