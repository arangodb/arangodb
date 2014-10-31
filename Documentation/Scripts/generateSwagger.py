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

import sys, re, json, string, os

rc = re.compile
MS = re.M | re.S

################################################################################
### @brief swagger
################################################################################

swagger = {
    'apiVersion': '0.1',
    'swaggerVersion': '1.1',
    'basePath': '/',
    'apis': []
    }

################################################################################
### @brief operation
################################################################################

operation = {}

################################################################################
### @brief C_FILE
################################################################################

C_FILE = False

################################################################################
### @brief DEBUG
################################################################################

DEBUG = False

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
    (l, c, line) = line.partition('{')
    (line, c , r) = line.rpartition('}')
    line = BackTicks(line, wordboundary = ['{','}'])

    return line

################################################################################
### @brief BackTicks
###
### `word` -> <b>word</b>
################################################################################

def BackTicks(txt, wordboundary = ['<em>','</em>']):
    r = rc(r"""([\(\s'/">]|^|.)\`(.*?)\`([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief AsteriskItalic
###
### *word* -> <b>word</b>
################################################################################

def AsteriskItalic(txt, wordboundary = ['<em>','</em>']):
    r = rc(r"""([\(\s'/">]|^|.)\*(.*?)\*([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief AsteriskBold
###
### **word** -> <b>word</b>
################################################################################

def AsteriskBold(txt, wordboundary = ['<em>','</em>']):
    r = rc(r"""([\(\s'/">]|^|.)\*\*(.*?)\*\*([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief FA
###
### @FA{word} -> <b>word</b>
################################################################################

def FA(txt, wordboundary = ['<b>','</b>']):
    r = rc(r"""([\(\s'/">]|^|.)@FA\{(.*?)\}([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief FN
###
### @FN{word} -> <b>word</b>
################################################################################

def FN(txt, wordboundary = ['<b>','</b>']):
    r = rc(r"""([\(\s'/">]|^|.)@FN\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief LIT
###
### @LIT{word} -> <b>word</b>
################################################################################

def LIT(txt, wordboundary = ['<b>','</b>']):
    r = rc(r"""([\(\s'/">]|^)@LIT\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief Typegraphy
################################################################################

def Typography(txt):
    if C_FILE:
        txt = txt[4:-1]
    else:
        txt = txt[0:-1]

    txt = BackTicks(txt)
    txt = AsteriskBold(txt)
    txt = AsteriskItalic(txt)
    txt = FN(txt)
    txt = LIT(txt)
    txt = FA(txt)

    # no way to find out the correct link for Swagger, 
    # so replace all @ref elements with just "the manual"

    r = rc(r"""@ref [a-zA-Z0-9]+""", MS)
    txt = r.sub("the manual", txt)
    txt = re.sub(r"@endDocuBlock", "", txt)

    return txt

################################################################################
### @brief InitializationError
################################################################################

class InitializationError(Exception): pass

################################################################################
### @brief StateMachine
################################################################################

class StateMachine:
    def __init__(self):
        self.handlers = []
        self.startState = None
        self.endStates = []

    def add_state(self, handler, end_state=0):
        self.handlers.append(handler)

        if end_state:
            self.endStates.append(handler)

    def set_start(self, handler):
        self.startState = handler

    def run(self, cargo=None):
        if not self.startState:
            raise InitializationError,\
                "must call .set_start() before .run()"

        if not self.endStates:
            raise InitializationError, \
                "at least one state must be an end_state"

        handler = self.startState

        while 1:
            (newState, cargo) = handler(cargo)

            if newState in self.endStates:
                newState(cargo)
                break
            elif newState not in self.handlers:
                raise RuntimeError, "Invalid target %s" % newState
            else:
                handler = newState

################################################################################
### @brief Regexen
################################################################################

class Regexen:
    def __init__(self):
        self.DESCRIPTION_LI = re.compile('^-\s.*$')
        self.DESCRIPTION_SP = re.compile('^\s\s.*$')
        self.DESCRIPTION_BL = re.compile('^\s*$')
        self.EMPTY_LINE = re.compile('^\s*$')
        self.END_EXAMPLE_ARANGOSH_RUN = re.compile('.*@END_EXAMPLE_ARANGOSH_RUN')
        self.EXAMPLES = re.compile('.*@EXAMPLES')
        self.EXAMPLE_ARANGOSH_RUN = re.compile('.*@EXAMPLE_ARANGOSH_RUN{')
        self.FILE = re.compile('.*@file')
        self.RESTBODYPARAM = re.compile('.*@RESTBODYPARAM')
        self.RESTDESCRIPTION = re.compile('.*@RESTDESCRIPTION')
        self.RESTDONE = re.compile('.*@RESTDONE')
        self.RESTHEADER = re.compile('.*@RESTHEADER{')
        self.RESTHEADERPARAM = re.compile('.*@RESTHEADERPARAM{')
        self.RESTHEADERPARAMETERS = re.compile('.*@RESTHEADERPARAMETERS')
        self.RESTQUERYPARAM = re.compile('.*@RESTQUERYPARAM{')
        self.RESTQUERYPARAMETERS = re.compile('.*@RESTQUERYPARAMETERS')
        self.RESTRETURNCODE = re.compile('.*@RESTRETURNCODE{')
        self.RESTRETURNCODES = re.compile('.*@RESTRETURNCODES')
        self.RESTURLPARAM = re.compile('.*@RESTURLPARAM{')
        self.RESTURLPARAMETERS = re.compile('.*@RESTURLPARAMETERS')
        self.NON_COMMENT = re.compile('^[^/].*')

################################################################################
### @brief checks for end of comment
################################################################################

def check_end_of_comment(line, r):
    if C_FILE:
        return r.NON_COMMENT.match(line)
    else:
        return r.RESTDONE.match(line)

################################################################################
### @brief next_step
################################################################################

def next_step(fp, line, r):
    global operation

    if not line:                              return eof, (fp, line)
    elif check_end_of_comment(line, r):       return skip_code, (fp, line)
    elif r.EXAMPLE_ARANGOSH_RUN.match(line):  return example_arangosh_run, (fp, line)
    elif r.RESTBODYPARAM.match(line):         return restbodyparam, (fp, line)
    elif r.RESTDESCRIPTION.match(line):       return restdescription, (fp, line)
    elif r.RESTHEADER.match(line):            return restheader, (fp, line)
    elif r.RESTHEADERPARAM.match(line):       return restheaderparam, (fp, line)
    elif r.RESTHEADERPARAMETERS.match(line):  return restheaderparameters, (fp, line)
    elif r.RESTQUERYPARAM.match(line):        return restqueryparam, (fp, line)
    elif r.RESTQUERYPARAMETERS.match(line):   return restqueryparameters, (fp, line)
    elif r.RESTRETURNCODE.match(line):        return restreturncode, (fp, line)
    elif r.RESTRETURNCODES.match(line):       return restreturncodes, (fp, line)
    elif r.RESTURLPARAM.match(line):          return resturlparam, (fp, line)
    elif r.RESTURLPARAMETERS.match(line):     return resturlparameters, (fp, line)

    if r.EXAMPLES.match(line):
        operation['examples'] = ""
        return examples, (fp, line)

    return None, None

################################################################################
### @brief generic handler
################################################################################

def generic_handler(cargo, r, message):
    global DEBUG

    if DEBUG: print >> sys.stderr, message
    (fp, last) = cargo

    while 1:
        (next, c) = next_step(fp, fp.readline(), r)

        if next:
            return next, c

################################################################################
### @brief generic handler with description
################################################################################

def generic_handler_desc(cargo, r, message, op, para, name):
    global DEBUG, C_FILE, operation

    if DEBUG: print >> sys.stderr, message
    (fp, last) = cargo

    inLI = False
    inUL = False

    while 1:
        line = fp.readline()
        (next, c) = next_step(fp, line, r)

        if next:
            para[name] = trim_text(para[name])

            if op:
                operation[op].append(para)

            return next, c

        if C_FILE and line[0:4] == "////":
            continue

        line = Typography(line)

        if r.DESCRIPTION_LI.match(line):
            line = "<li>" + line[2:]
            inLI = True
        elif inLI and r.DESCRIPTION_SP.match(line):
            line = line[2:]
        elif inLI and r.DESCRIPTION_BL.match(line):
            line = ""
        else:
            inLI = False

        if not inUL and inLI:
            line = " <ul class=\"swagger-list\">" + line
            inUL = True
        elif inUL and not inLI:
            line = "</ul> " + line
            inUL = False

        if not inLI and r.EMPTY_LINE.match(line):
            line = "<br><br>"

        para[name] += line + ' '

################################################################################
### @brief restheader
################################################################################

def restheader(cargo, r=Regexen()):
    global swagger, operation

    (fp, last) = cargo

    temp = parameters(last).split(',')
    (method, path) = temp[0].split()

    summary = temp[1]
    summaryList = summary.split()

    nickname = summaryList[0] + ''.join([word.capitalize() for word in summaryList[1:]])

    operation = {
        'httpMethod': method,
        'nickname': nickname,
        'parameters': [],
        'summary': summary,
        'notes': '',
        'examples': '',
        'errorResponses': []
        }
    
    api = {
        'path': FA(path, wordboundary = ['{', '}']),
        'operations': [ operation ]
        }

    swagger['apis'].append(api)

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
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    if parametersList[2] == 'required':
        required = 'true'
    else:
        required = 'false'
        
    para = {
        'name': parametersList[0],
        'paramType': 'path',
        'description': '',
        'dataType': parametersList[1].capitalize(),
        'required': required
        }

    return generic_handler_desc(cargo, r, "resturlparam", 'parameters', para, 'description')

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
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    para = {
        'paramType': 'header',
        'dataType': parametersList[1].capitalize(),
        'name': parametersList[0],
        'description': ''
        }

    return generic_handler_desc(cargo, r, "restheaderparam", 'parameters', para, 'description')

################################################################################
### @brief restbodyparam
################################################################################

def restbodyparam(cargo, r=Regexen()):
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    if parametersList[2] == 'required':
        required = 'true'
    else:
        required = 'false'    

    para = {
        'name': parametersList[0],
        'paramType': 'body',
        'description': '',
        'dataType': parametersList[1].capitalize(),
        'required': required
        }

    return generic_handler_desc(cargo, r, "restbodyparam", 'parameters', para, 'description')

################################################################################
### @brief restqueryparam
################################################################################

def restqueryparam(cargo, r=Regexen()):
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    if parametersList[2] == 'required':
        required = 'true'
    else:
        required = 'false'

    para = {
        'name': parametersList[0],
        'paramType': 'query',
        'description': '',
        'dataType': parametersList[1].capitalize(),
        'required': required
        }

    return generic_handler_desc(cargo, r, "restqueryparam", 'parameters', para, 'description')

################################################################################
### @brief restdescription
################################################################################

def restdescription(cargo, r=Regexen()):
    return generic_handler_desc(cargo, r, "restdescription", None, operation, 'notes')

################################################################################
### @brief restreturncodes
################################################################################

def restreturncodes(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restreturncodes")

################################################################################
### @brief restreturncode
################################################################################

def restreturncode(cargo, r=Regexen()):
    (fp, last) = cargo

    returncode = {
        'code': parameters(last),
        'reason': ''
        }
        
    return generic_handler_desc(cargo, r, "restreturncode", 'errorResponses', returncode, 'reason')

################################################################################
### @brief examples
################################################################################

def examples(cargo, r=Regexen()):
    return generic_handler_desc(cargo, r, "examples", None, operation, 'examples')

################################################################################
### @brief example_arangosh_run
################################################################################

def example_arangosh_run(cargo, r=Regexen()):
    global DEBUG, C_FILE

    if DEBUG: print >> sys.stderr, "example_arangosh_run"
    fp, last = cargo

    # new examples code TODO should include for each example own object in json file
    examplefile = open(os.path.join(os.path.dirname(__file__), '../Examples/' + parameters(last) + '.generated'))

    operation['examples'] += '<br><br><pre><code class="json">'

    for line in examplefile.readlines():
        operation['examples'] += line

    operation['examples'] += '</code></pre><br>'

    line = ""

    while not r.END_EXAMPLE_ARANGOSH_RUN.match(line):
        line = fp.readline()

        if not line:
            return eof, (fp, line)

    return examples, (fp, line)

################################################################################
### @brief eof
################################################################################

def eof(cargo):
    global DEBUG, C_FILE
    if DEBUG: print >> sys.stderr, "eof"

    print json.dumps(swagger, indent=4, separators=(',',': '))

################################################################################
### @brief error
################################################################################

def error(cargo):
    global DEBUG, C_FILE
    if DEBUG: print >> sys.stderr, "error"

    sys.stderr.write('Unidentifiable line:\n' + cargo)

################################################################################
### @brief comment
################################################################################

def comment(cargo, r=Regexen()):
    global DEBUG, C_FILE

    if DEBUG: print >> sys.stderr, "comment"
    (fp, last) = cargo

    while 1:
        line = fp.readline()
        if not line: return eof, (fp, line)

        if r.FILE.match(line): C_FILE = True

        next, c = next_step(fp, line, r)

        if next:
            return next, c

################################################################################
### @brief skip_code
###
### skip all non comment lines
################################################################################

def skip_code(cargo, r=Regexen()):
    global DEBUG, C_FILE

    if DEBUG: print >> sys.stderr, "skip_code"
    (fp, last) = cargo
    
    if not C_FILE:
        return comment((fp, last), r)

    while 1:
        line = fp.readline()

        if not line:
            return eof, (fp, line)

        if not r.NON_COMMENT.match(line):
            return comment((fp, line), r)

################################################################################
### @brief main
################################################################################

if __name__ == "__main__":
    automat = StateMachine()

    automat.add_state(comment)
    automat.add_state(eof, end_state=1)
    automat.add_state(error, end_state=1)
    automat.add_state(example_arangosh_run)
    automat.add_state(examples)
    automat.add_state(skip_code)
    automat.add_state(restbodyparam)
    automat.add_state(restdescription)
    automat.add_state(restheader)
    automat.add_state(restheaderparam)
    automat.add_state(restheaderparameters)
    automat.add_state(restqueryparam)
    automat.add_state(restqueryparameters)
    automat.add_state(restreturncode)
    automat.add_state(restreturncodes)
    automat.add_state(resturlparam)
    automat.add_state(resturlparameters)

    automat.set_start(skip_code)
    automat.run((sys.stdin, ''))

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
