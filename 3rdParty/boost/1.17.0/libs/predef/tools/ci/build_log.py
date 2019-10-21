#!/usr/bin/env python

# Copyright 2008 Rene Rivera
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

import re
import optparse
import time
import xml.dom.minidom
import xml.dom.pulldom
from xml.sax.saxutils import unescape, escape
import os.path
from pprint import pprint
from __builtin__ import exit

class BuildOutputXMLParsing(object):
    '''
    XML parsing utilities for dealing with the Boost Build output
    XML format.
    '''
    
    def get_child_data( self, root, tag = None, id = None, name = None, strip = False, default = None ):
        return self.get_data(self.get_child(root,tag=tag,id=id,name=name),strip=strip,default=default)
    
    def get_data( self, node, strip = False, default = None ):
        data = None
        if node:
            data_node = None
            if not data_node:
                data_node = self.get_child(node,tag='#text')
            if not data_node:
                data_node = self.get_child(node,tag='#cdata-section')
            data = ""
            while data_node:
                data += data_node.data
                data_node = data_node.nextSibling
                if data_node:
                    if data_node.nodeName != '#text' \
                        and data_node.nodeName != '#cdata-section':
                        data_node = None
        if not data:
            data = default
        else:
            if strip:
                data = data.strip()
        return data
    
    def get_child( self, root, tag = None, id = None, name = None, type = None ):
        return self.get_sibling(root.firstChild,tag=tag,id=id,name=name,type=type)
    
    def get_sibling( self, sibling, tag = None, id = None, name = None, type = None ):
        n = sibling
        while n:
            found = True
            if type and found:
                found = found and type == n.nodeType
            if tag and found:
                found = found and tag == n.nodeName
            if (id or name) and found:
                found = found and n.nodeType == xml.dom.Node.ELEMENT_NODE
            if id and found:
                if n.hasAttribute('id'):
                    found = found and n.getAttribute('id') == id
                else:
                    found = found and n.hasAttribute('id') and n.getAttribute('id') == id
            if name and found:
                found = found and n.hasAttribute('name') and n.getAttribute('name') == name
            if found:
                return n
            n = n.nextSibling
        return None

class BuildOutputProcessor(BuildOutputXMLParsing):
    
    def __init__(self, inputs):
        self.test = {}
        self.target_to_test = {}
        self.target = {}
        self.parent = {}
        self.timestamps = []
        for input in inputs:
            self.add_input(input)
    
    def add_input(self, input):
        '''
        Add a single build XML output file to our data.
        '''
        events = xml.dom.pulldom.parse(input)
        context = []
        for (event,node) in events:
            if event == xml.dom.pulldom.START_ELEMENT:
                context.append(node)
                if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                    x_f = self.x_name_(*context)
                    if x_f:
                        events.expandNode(node)
                        # expanding eats the end element, hence walking us out one level
                        context.pop()
                        # call handler
                        (x_f[1])(node)
            elif event == xml.dom.pulldom.END_ELEMENT:
                context.pop()
    
    def x_name_(self, *context, **kwargs):
        node = None
        names = [ ]
        for c in context:
            if c:
                if not isinstance(c,xml.dom.Node):
                    suffix = '_'+c.replace('-','_').replace('#','_')
                else:
                    suffix = '_'+c.nodeName.replace('-','_').replace('#','_')
                    node = c
                names.append('x')
                names = map(lambda x: x+suffix,names)
        if node:
            for name in names:
                if hasattr(self,name):
                    return (name,getattr(self,name))
        return None
    
    def x_build_test(self, node):
        '''
        Records the initial test information that will eventually
        get expanded as we process the rest of the results.
        '''
        test_node = node
        test_name = test_node.getAttribute('name')
        test_target = self.get_child_data(test_node,tag='target',strip=True)
        ## print ">>> %s %s" %(test_name,test_target)
        self.test[test_name] = {
            'library' : "/".join(test_name.split('/')[0:-1]),
            'test-name' : test_name.split('/')[-1],
            'test-type' : test_node.getAttribute('type').lower(),
            'test-program' : self.get_child_data(test_node,tag='source',strip=True),
            'target' : test_target,
            'info' : self.get_child_data(test_node,tag='info',strip=True),
            'dependencies' : [],
            'actions' : [],
            }
        # Add a lookup for the test given the test target.
        self.target_to_test[self.test[test_name]['target']] = test_name
        return None
    
    def x_build_targets_target( self, node ):
        '''
        Process the target dependency DAG into an ancestry tree so we can look up
        which top-level library and test targets specific build actions correspond to.
        '''
        target_node = node
        name = self.get_child_data(target_node,tag='name',strip=True)
        path = self.get_child_data(target_node,tag='path',strip=True)
        jam_target = self.get_child_data(target_node,tag='jam-target',strip=True)
        #~ Map for jam targets to virtual targets.
        self.target[jam_target] = {
            'name' : name,
            'path' : path
            }
        #~ Create the ancestry.
        dep_node = self.get_child(self.get_child(target_node,tag='dependencies'),tag='dependency')
        while dep_node:
            child = self.get_data(dep_node,strip=True)
            child_jam_target = '<p%s>%s' % (path,child.split('//',1)[1])
            self.parent[child_jam_target] = jam_target
            dep_node = self.get_sibling(dep_node.nextSibling,tag='dependency')
        return None
    
    def x_build_action( self, node ):
        '''
        Given a build action log, process into the corresponding test log and
        specific test log sub-part.
        '''
        action_node = node
        name = self.get_child(action_node,tag='name')
        if name:
            name = self.get_data(name)
            #~ Based on the action, we decide what sub-section the log
            #~ should go into.
            action_type = None
            if re.match('[^%]+%[^.]+[.](compile)',name):
                action_type = 'compile'
            elif re.match('[^%]+%[^.]+[.](link|archive)',name):
                action_type = 'link'
            elif re.match('[^%]+%testing[.](capture-output)',name):
                action_type = 'run'
            elif re.match('[^%]+%testing[.](expect-failure|expect-success)',name):
                action_type = 'result'
            else:
                # TODO: Enable to see what other actions can be included in the test results.
                # action_type = None
                action_type = 'other'
            #~ print "+   [%s] %s %s :: %s" %(action_type,name,'','')
            if action_type:
                #~ Get the corresponding test.
                (target,test) = self.get_test(action_node,type=action_type)
                #~ Skip action that have no corresponding test as they are
                #~ regular build actions and don't need to show up in the
                #~ regression results.
                if not test:
                    ##print "??? [%s] %s %s :: %s" %(action_type,name,target,test)
                    return None
                ##print "+++ [%s] %s %s :: %s" %(action_type,name,target,test)
                #~ Collect some basic info about the action.
                action = {
                    'command' : self.get_action_command(action_node,action_type),
                    'output' : self.get_action_output(action_node,action_type),
                    'info' : self.get_action_info(action_node,action_type)
                    }
                #~ For the test result status we find the appropriate node
                #~ based on the type of test. Then adjust the result status
                #~ accordingly. This makes the result status reflect the
                #~ expectation as the result pages post processing does not
                #~ account for this inversion.
                action['type'] = action_type
                if action_type == 'result':
                    if re.match(r'^compile',test['test-type']):
                        action['type'] = 'compile'
                    elif re.match(r'^link',test['test-type']):
                        action['type'] = 'link'
                    elif re.match(r'^run',test['test-type']):
                        action['type'] = 'run'
                #~ The result sub-part we will add this result to.
                if action_node.getAttribute('status') == '0':
                    action['result'] = 'succeed'
                else:
                    action['result'] = 'fail'
                # Add the action to the test.
                test['actions'].append(action)
                # Set the test result if this is the result action for the test.
                if action_type == 'result':
                    test['result'] = action['result']
        return None
    
    def x_build_timestamp( self, node ):
        '''
        The time-stamp goes to the corresponding attribute in the result.
        '''
        self.timestamps.append(self.get_data(node).strip())
        return None
    
    def get_test( self, node, type = None ):
        '''
        Find the test corresponding to an action. For testing targets these
        are the ones pre-declared in the --dump-test option. For libraries
        we create a dummy test as needed.
        '''
        jam_target = self.get_child_data(node,tag='jam-target')
        base = self.target[jam_target]['name']
        target = jam_target
        while target in self.parent:
            target = self.parent[target]
        #~ print "--- TEST: %s ==> %s" %(jam_target,target)
        #~ main-target-type is a precise indicator of what the build target is
        #~ originally meant to be.
        #main_type = self.get_child_data(self.get_child(node,tag='properties'),
        #    name='main-target-type',strip=True)
        main_type = None
        if main_type == 'LIB' and type:
            lib = self.target[target]['name']
            if not lib in self.test:
                self.test[lib] = {
                    'library' : re.search(r'libs/([^/]+)',lib).group(1),
                    'test-name' : os.path.basename(lib),
                    'test-type' : 'lib',
                    'test-program' : os.path.basename(lib),
                    'target' : lib
                    }
            test = self.test[lib]
        else:
            target_name_ = self.target[target]['name']
            if self.target_to_test.has_key(target_name_):
                test = self.test[self.target_to_test[target_name_]]
            else:
                test = None
        return (base,test)
    
    #~ The command executed for the action. For run actions we omit the command
    #~ as it's just noise.
    def get_action_command( self, action_node, action_type ):
        if action_type != 'run':
            return self.get_child_data(action_node,tag='command')
        else:
            return ''
    
    #~ The command output.
    def get_action_output( self, action_node, action_type ):
        return self.get_child_data(action_node,tag='output',default='')
    
    #~ Some basic info about the action.
    def get_action_info( self, action_node, action_type ):
        info = {}
        #~ The jam action and target.
        info['name'] = self.get_child_data(action_node,tag='name')
        info['path'] = self.get_child_data(action_node,tag='path')
        #~ The timing of the action.
        info['time-start'] = action_node.getAttribute('start')
        info['time-end'] = action_node.getAttribute('end')
        info['time-user'] = action_node.getAttribute('user')
        info['time-system'] = action_node.getAttribute('system')
        #~ Testing properties.
        test_info_prop = self.get_child_data(self.get_child(action_node,tag='properties'),name='test-info')
        info['always_show_run_output'] = test_info_prop == 'always_show_run_output'
        #~ And for compiles some context that may be hidden if using response files.
        if action_type == 'compile':
            info['define'] = []
            define = self.get_child(self.get_child(action_node,tag='properties'),name='define')
            while define:
                info['define'].append(self.get_data(define,strip=True))
                define = self.get_sibling(define.nextSibling,name='define')
        return info

class BuildConsoleSummaryReport(object):
    
    HEADER = '\033[35m\033[1m'
    INFO = '\033[34m'
    OK = '\033[32m'
    WARNING = '\033[33m'
    FAIL = '\033[31m'
    ENDC = '\033[0m'
    
    def __init__(self, bop, opt):
        self.bop = bop
    
    def generate(self):
        self.summary_info = {
            'total' : 0,
            'success' : 0,
            'failed' : [],
            }
        self.header_print("======================================================================")
        self.print_test_log()
        self.print_summary()
        self.header_print("======================================================================")
    
    @property
    def failed(self):
        return len(self.summary_info['failed']) > 0
    
    def print_test_log(self):
        self.header_print("Tests run..")
        self.header_print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
        for k in sorted(self.bop.test.keys()):
            test = self.bop.test[k]
            if len(test['actions']) > 0:
                self.summary_info['total'] += 1
                ##print ">>>> {0}".format(test['test-name'])
                if 'result' in test:
                    succeed = test['result'] == 'succeed'
                else:
                    succeed = test['actions'][-1]['result'] == 'succeed'
                if succeed:
                    self.summary_info['success'] += 1
                else:
                    self.summary_info['failed'].append(test)
                if succeed:
                    self.ok_print("[PASS] {0}",k)
                else:
                    self.fail_print("[FAIL] {0}",k)
                for action in test['actions']:
                    self.print_action(succeed, action)
    
    def print_action(self, test_succeed, action):
        '''
        Print the detailed info of failed or always print tests.
        '''
        #self.info_print(">>> {0}",action.keys())
        if not test_succeed or action['info']['always_show_run_output']:
            output = action['output'].strip()
            if output != "":
                p = self.fail_print if action['result'] == 'fail' else self.p_print
                self.info_print("")
                self.info_print("({0}) {1}",action['info']['name'],action['info']['path'])
                p("")
                p("{0}",action['command'].strip())
                p("")
                for line in output.splitlines():
                    p("{0}",line.encode('utf-8'))
    
    def print_summary(self):
        self.header_print("")
        self.header_print("Testing summary..")
        self.header_print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
        self.p_print("Total: {0}",self.summary_info['total'])
        self.p_print("Success: {0}",self.summary_info['success'])
        if self.failed:
            self.fail_print("Failed: {0}",len(self.summary_info['failed']))
            for test in self.summary_info['failed']:
                self.fail_print("  {0}/{1}",test['library'],test['test-name'])
    
    def p_print(self, format, *args, **kargs):
        print format.format(*args,**kargs)
    
    def info_print(self, format, *args, **kargs):
        print self.INFO+format.format(*args,**kargs)+self.ENDC
    
    def header_print(self, format, *args, **kargs):
        print self.HEADER+format.format(*args,**kargs)+self.ENDC
    
    def ok_print(self, format, *args, **kargs):
        print self.OK+format.format(*args,**kargs)+self.ENDC
    
    def warn_print(self, format, *args, **kargs):
        print self.WARNING+format.format(*args,**kargs)+self.ENDC
    
    def fail_print(self, format, *args, **kargs):
        print self.FAIL+format.format(*args,**kargs)+self.ENDC

class Main(object):
    
    def __init__(self,args=None):
        op = optparse.OptionParser(
            usage="%prog [options] input+")
        op.add_option( '--output',
            help="type of output to generate" )
        ( opt, inputs ) = op.parse_args(args)
        bop = BuildOutputProcessor(inputs)
        output = None
        if opt.output == 'console':
            output = BuildConsoleSummaryReport(bop, opt)
        if output:
            output.generate()
            self.failed = output.failed

if __name__ == '__main__':
    m = Main()
    if m.failed:
        exit(-1)
