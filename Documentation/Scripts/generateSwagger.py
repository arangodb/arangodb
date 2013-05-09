#!/usr/bin/python

# -*- coding: utf-8 -*-

################################################################################
### @brief creates swagger json files from doc headers of rest files
###
### find files in
###   arangod/RestHandler/*.cpp
###   js/actions/api-*.js
### @usage generateSwagger.py < RestXXXX.cpp > restSwagger.json
###
### @file
###
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
### @author Thomas Richter
### @author Copyright 2013, triAGENS GmbH, Cologne, Germany
################################################################################

import sys, re, json, string
operation = {}
errorResponse = { 'code': None, 'reason': None }
# api = {'path': '/_api/document', 'description': '', 'operations': []}
swagger = {'apiVersion': '0.1',
					'swaggerVersion': '1.1',
					'basePath': '/',
					'apis': [],
					}

rc = re.compile
MS = re.M | re.S

def parameters(line):
    # suche die erste {
    # suche die letzten }
    # gib alles dazwischen zurck
    l, c, line =line.partition('{')
    line , c , r = line.rpartition('}')
    line = BackTicks(line, wordboundary = ['{','}'])
    return line

def BackTicks(txt, wordboundary = ['<em>','</em>']):
    # `word` -> <b>word</b>
    r = rc(r"""([\(\s'/">]|^|.)\`(.*?)\`([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'
    return r.sub(subpattern, txt)

def FA(txt, wordboundary = ['<b>','</b>']):
    # @FA{word} -> <b>word</b>
    r = rc(r"""([\(\s'/">]|^|.)@FA\{(.*?)\}([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'
    return r.sub(subpattern, txt)

def FN(txt, wordboundary = ['<b>','</b>']):
    # @FN{word} -> <b>word</b>
    r = rc(r"""([\(\s'/">]|^|.)@FN\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'
    return r.sub(subpattern, txt)

def LIT(txt, wordboundary = ['<b>','</b>']):
    # @LIT{word} -> <b>word</b>
    r = rc(r"""([\(\s'/">]|^)@LIT\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'
    return r.sub(subpattern, txt)

def Typography(txt):
    txt = BackTicks(txt)
    txt = FN(txt)
    txt = LIT(txt)
    txt = FA(txt)
    return txt

class InitializationError(Exception): pass

# idea from http://gnosis.cx/TPiP/chap4.txt
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

class Regexen:
    def __init__(self):
        self.brief = re.compile('.*@brief')
        self.RESTHEADER = re.compile('.*@RESTHEADER{')
        self.RESTURLPARAMETERS = re.compile('.*@RESTURLPARAMETERS')
        self.RESTQUERYPARAMETERS = re.compile('.*@RESTQUERYPARAMETERS')
        self.RESTHEADERPARAMETERS = re.compile('.*@RESTHEADERPARAMETERS')
        self.RESTHEADERPARAM = re.compile('.*@RESTHEADERPARAM{')
        self.RESTBODYPARAM = re.compile('.*@RESTBODYPARAM')
        self.RESTURLPARAM = re.compile('.*@RESTURLPARAM{')
        self.RESTQUERYPARAM = re.compile('.*@RESTQUERYPARAM{')
        self.RESTDESCRIPTION = re.compile('.*@RESTDESCRIPTION')
        self.RESTRETURNCODES = re.compile('.*@RESTRETURNCODES')
        self.RESTRETURNCODE = re.compile('.*@RESTRETURNCODE{')
        self.EXAMPLES = re.compile('.*@EXAMPLES')
        self.EXAMPLE_ARANGOSH_RUN = re.compile('.*@EXAMPLE_ARANGOSH_RUN{|.*@verbinclude.*')
        self.END_EXAMPLE_ARANGOSH_RUN = re.compile('.*@END_EXAMPLE_ARANGOSH_RUN')
        self.read_through = re.compile('^[^/].*')
        self.EMPTY_COMMENT = re.compile('^/{3}\s*$')
        self.DESCRIPTION_LI = re.compile('^/{3}\s-.*$')

# state maschine rules
def resturlparameters(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.RESTURLPARAM.match(line):             return resturlparam, (fp, line)
        elif r.RESTQUERYPARAMETERS.match(line):      return restqueryparameters, (fp, line)
        elif r.RESTHEADERPARAMETERS.match(line):     return restheaderparameters, (fp, line)
        elif r.RESTBODYPARAM.match(line):            return restbodyparam, (fp, line)
        elif r.RESTDESCRIPTION.match(line):          return restdescription, (fp, line)
        else:										 continue

def resturlparam(cargo, r=Regexen()):
    fp, last = cargo
    parametersList = parameters(last).split(',')
    para = {}
    para['paramType'] = 'path'
    para['dataType'] = parametersList[1].capitalize()
    if parametersList[2] == 'required':
        para['required'] = 'true'
    else:
        para['required'] = 'false'
    para['name'] = parametersList[0]
    para['description']=''
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.EMPTY_COMMENT.match(line):
            operation['parameters'].append(para)
            return resturlparameters, (fp, line)
        else:
            para['description'] += Typography(line[4:-1]) + ' '

def restqueryparameters(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.RESTURLPARAMETERS.match(line):        return resturlparameters, (fp, line)
        elif r.RESTHEADERPARAMETERS.match(line):     return restheaderparameters, (fp, line)
        elif r.RESTBODYPARAM.match(line):            return restbodyparam, (fp, line)
        elif r.RESTDESCRIPTION.match(line):          return restdescription, (fp, line)
        elif r.RESTQUERYPARAM.match(line):           return restqueryparam, (fp, line)
        else:										 continue

def restheaderparameters(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.RESTHEADERPARAM.match(line):          return restheaderparam, (fp, line)
        elif r.RESTQUERYPARAMETERS.match(line):      return restqueryparameters, (fp, line)
        elif r.RESTURLPARAMETERS.match(line):        return resturlparameters, (fp, line)
        elif r.RESTBODYPARAM.match(line):            return restbodyparam, (fp, line)
        elif r.RESTDESCRIPTION.match(line):          return restdescription, (fp, line)
        else:										 continue

def restheaderparam(cargo, r=Regexen()):
    fp, last = cargo
    parametersList = parameters(last).split(',')
    para = {}
    para['paramType'] = 'header'
    para['dataType'] = parametersList[1].capitalize()
    para['name'] = parametersList[0]
    para['description']=''
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.EMPTY_COMMENT.match(line):
            operation['parameters'].append(para)
            return restheaderparameters, (fp, line)
        else:
            para['description'] += Typography(line[4:-1]) + ' '

def restbodyparam(cargo, r=Regexen()):
    fp, last = cargo
    parametersList = parameters(last).split(',')
    para = {}
    para['paramType'] = 'body'
    para['dataType'] = parametersList[1].capitalize()
    if parametersList[2] == 'required':
        para['required'] = 'true'
    para['name'] = parametersList[0]
    para['description']=''
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.RESTURLPARAMETERS.match(line):        return resturlparameters, (fp, line)
        elif r.RESTHEADERPARAMETERS.match(line):     return restheaderparameters, (fp, line)
        elif r.RESTQUERYPARAMETERS.match(line):      return restqueryparameters, (fp, line)
        elif r.RESTDESCRIPTION.match(line):          return restdescription, (fp, line)
        elif r.EMPTY_COMMENT.match(line):
            operation['parameters'].append(para)
            return comment, (fp, line)
        else:
            para['description'] += Typography(line[4:-1]) + ' '

def restqueryparam(cargo, r=Regexen()):
    fp, last = cargo
    parametersList = parameters(last).split(',')
    para = {}
    para['paramType'] = 'query'
    para['dataType'] = parametersList[1].capitalize()
    if parametersList[2] == 'required':
        para['required'] = 'True'
    para['name'] = parametersList[0]
    para['description']=''
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.EMPTY_COMMENT.match(line):
            operation['parameters'].append(para)
            return restqueryparameters, (fp, line)
        else:
            para['description'] += Typography(line[4:-1]) + ' '

def restdescription(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.EMPTY_COMMENT.match(line):
            if r.DESCRIPTION_LI.match(last):
                operation['notes'] += '<br>'
            else:
                operation['notes'] += '<br><br>'
        elif r.DESCRIPTION_LI.match(line):           operation['notes'] += Typography(line[4:-1]) + '<br>'
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.EXAMPLES.match(line):
            return examples, (fp, line)
        elif r.RESTRETURNCODES.match(line):				   return restreturncodes, (fp, line)
        else:
            operation['notes'] += Typography(line[4:-1]) + ' '
        last = line

def restreturncodes(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.RESTRETURNCODE.match(line):           return restreturncode, (fp, line)
        elif r.EXAMPLES.match(line):
            operation['examples']= ""
            return examples, (fp, line)
        else: continue

def restreturncode(cargo, r=Regexen()):
    fp, last = cargo
    returncode = {}
    returncode['code'] = parameters(last)
    returncode['reason'] = ''

    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.EXAMPLES.match(line):
            operation['examples'] = ''
            return examples, (fp, line)
        elif r.EMPTY_COMMENT.match(line):
            operation['errorResponses'].append(returncode)
            return restreturncodes, (fp, line)
        else:
            returncode['reason'] += Typography(line[4:-1]) + ' '

def examples(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.EXAMPLE_ARANGOSH_RUN.match(line):     return example_arangosh_run, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)
        elif r.EMPTY_COMMENT.match(line):            continue
        elif len(line) >= 4 and line[:4] == "////":  continue
        else:
            operation['examples'] += Typography(line[4:-1]) + ' '

def example_arangosh_run(cargo, r=Regexen()):
    fp, last = cargo
    import os
    # old examples code
    verbinclude = last[4:-1].split()[0] == "@verbinclude"
    if verbinclude:
        examplefile = open(os.path.join(os.path.dirname(__file__), '../Examples/' + last[4:-1].split()[1]))
    else:
        # new examples code TODO should include for each example own object in json file
        examplefile = open(os.path.join(os.path.dirname(__file__), '../Examples/' + parameters(last) + '.generated'))
    operation['examples'] += '<br><br><pre><code class="json" >'
    for line in examplefile.readlines():
        operation['examples'] += line
    operation['examples'] += '</code></pre><br>'
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        elif r.END_EXAMPLE_ARANGOSH_RUN.match(line) or verbinclude:
            return examples, (fp, line)
        elif r.read_through.match(line):             return read_through, (fp, line)


def eof(cargo):
		print json.dumps(swagger, indent=4, separators=(',',': '))

def error(cargo):
    sys.stderr.write('Unidentifiable line:\n' + line)

def comment(cargo, r=Regexen()):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:                                 return eof, (fp, line)
        # elif r.brief.match(line):										print line[4:-1]
        elif r.RESTHEADER.match(line):
            temp = parameters(line).split(',')
            method, path = temp[0].split()
            summary = temp[1]
            '# create new api'
            api = {}
            api['path'] = FA(path, wordboundary = ['{', '}'])
            api['operations']=[]
            swagger['apis'].append(api)
            _operation = { 'httpMethod': None, 'nickname': None, 'parameters': [],
                'summary': None, 'notes': '', 'examples': '', 'errorResponses':[]}
            _operation['httpMethod'] = method
            summaryList = summary.split()
            _operation['nickname'] = summaryList[0] + ''.join([word.capitalize() for word in summaryList[1:]])
            _operation['summary'] = summary
            api['operations'].append(_operation)
            global operation
            operation = _operation
        elif r.RESTURLPARAMETERS.match(line):        return resturlparameters, (fp, line)
        elif r.RESTHEADERPARAMETERS.match(line):     return restheaderparameters, (fp, line)
        elif r.RESTBODYPARAM.match(line):            return restbodyparam, (fp, line)
        elif r.RESTQUERYPARAMETERS.match(line):      return restqueryparameters, (fp, line)
        elif r.RESTDESCRIPTION.match(line):          return restdescription, (fp, line)
        elif len(line) >= 4 and line[:4] == "////":  continue
        elif len(line) >= 3 and line[:3] =="///":    continue
        else:                                        return read_through, (fp, line)

# skip all non comment lines
def read_through(cargo):
    fp, last = cargo
    while 1:
        line = fp.readline()
        if not line:          return eof, (fp, line)
        elif len(line) >= 3 and line[:3] == "///": return comment, (fp, line)
        else:                 continue

if __name__ == "__main__":
    automat = StateMachine()
    automat.add_state(read_through)
    automat.add_state(eof, end_state=1)
    automat.add_state(comment)
    automat.add_state(resturlparameters)
    automat.add_state(resturlparam)
    automat.add_state(restqueryparameters)
    automat.add_state(restqueryparam)
    automat.add_state(restbodyparam)
    automat.add_state(restheaderparameters)
    automat.add_state(restheaderparam)
    automat.add_state(restdescription)
    automat.add_state(restreturncodes)
    automat.add_state(restreturncode)
    automat.add_state(examples)
    automat.add_state(example_arangosh_run)
    automat.add_state(error, end_state=1)
    automat.set_start(read_through)
    automat.run((sys.stdin, ''))
# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
