################################################################################
### @brief create a C stub from a Python file
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
### @author Dr. Frank Celler
### @author Copyright 2011, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, string

file_name = sys.argv[1]

DEBUG = False

################################################################################
### @brief parse file
################################################################################

r1 = re.compile(r'//')
r2 = re.compile(r'^\s*function\s*([a-zA-Z0-9_]*)\s*\((.*)\)\s*{')
r3 = re.compile(r'^\s*$')
r4 = re.compile(r'^\s*([a-zA-Z0-9\._]*)\s*=\s*function\s*\((.*)\)\s*{')
r5 = re.compile(r'^\s*actions\.defineHttp\(')
r6 = re.compile(r'^/// @fn ([a-zA-Z0-9_]*)\s*')

f = open(file_name, "r")

comment = False
other = True
count = 0;
fn = None

for line in f:
    line = line.rstrip('\n')
    count = count + 1

    # check for action name
    m = r6.match(line)

    if m:
        fn = m.group(1)
        continue

    # check for comments
    m = r1.match(line)

    if m:
        if not comment and not other:
            if fn:
                print "void %s ();\n" % fn
            else:
                print "void dummy_%d ();\n" % count
            
        print "%s" % line

        comment = True
        other = False
        continue

    comment = False

    # check for empty lines
    m = r3.match(line)

    if m:
        print "%s" % line
        continue

    # check for function definition
    m = r2.match(line)

    if m:
	if fn:
            func = "void " + fn + " ("
	    fn = None
	else:
            name = m.group(1)
            func = "void JSF_" + name + " ("

        args = m.group(2).split(',')
        sep = ""

        if 1 == len(args) and args[0] == "":
            args = []

        for a in args:
            a = a.strip()
            func = func + sep + "int " + a;
            sep = ", "

        func = func + ")"

        print "%s {}" % func
        other = True
        continue

    # check for function assigment
    m = r4.match(line)

    if m:
	if fn:
            func = "void " + fn + " ("
	    fn = None
	else:
            name = m.group(1)
            func = "void JSF_" + string.replace(name, '.', '_') + " ("

        args = m.group(2).split(',')
        sep = ""

        for a in args:
            a = a.strip()
            func = func + sep + "int " + a;
            sep = ", "

        func = func + ")"

        print "%s {}" % func
        other = True
        continue

    # check for action definition
    m = r5.match(line)

    if m and fn:
        func = "void " + fn + " ()"
        fn = None

        print "%s {}" % func
        other = True
        continue

f.close()
