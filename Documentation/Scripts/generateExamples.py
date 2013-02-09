################################################################################
### @brief creates examples from documentation files
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
### @author Copyright 2011-2012, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, string

argv = sys.argv
argv.pop(0)

################################################################################
### @brief enable debug output
################################################################################

DEBUG = False

################################################################################
### @brief output directory
################################################################################

OutputDir = "/tmp/"

################################################################################
### @brief single line arangosh output
################################################################################

ArangoshExamples = {}

################################################################################
### @brief global setup for arangosh
################################################################################

ArangoshSetup = ""

################################################################################
### @brief states
################################################################################

STATE_BEGIN = 0
STATE_EXAMPLE_ARANGOSH = 1

state = STATE_BEGIN

################################################################################
### @brief parse file
################################################################################

r1 = re.compile(r'^@EXAMPLE_ARANGOSH_OUTPUT{([^}]*)}')
r2 = re.compile(r'^@END_EXAMPLE_ARANGOSH_OUTPUT')

name = ""

STATE_NORMAL = 0
STATE_ARANGOSH_SETUP = 1
STATE_OUTPUT_DIR = 2

fstate = STATE_NORMAL

for filename in argv:
    if filename == "--arangosh-setup":
        fstate = STATE_ARANGOSH_SETUP
        continue

    if filename == "--output-dir":
        fstate = STATE_OUTPUT_DIR
        continue

    ## .............................................................................
    ## input file
    ## .............................................................................
    
    if fstate == STATE_NORMAL:
        f = open(filename, "r")
        state = STATE_BEGIN

        for line in f:
            line = line.rstrip('\n')

            if state == STATE_BEGIN:
                m = r1.match(line)

                if m:
                    name = m.group(1)
                    state = STATE_EXAMPLE_ARANGOSH
                    continue

            elif state == STATE_EXAMPLE_ARANGOSH:
                m = r2.match(line)

                if m:
                    name = ""
                    state = STATE_BEGIN
                    continue

                if not name in ArangoshExamples:
                    ArangoshExamples[name] = ""

                ArangoshExamples[name] += line + "\n"

        f.close()

    ## .............................................................................
    ## arangosh setup file
    ## .............................................................................

    elif fstate == STATE_EXAMPLE_ARANGOSH:
        fstate = STATE_NORMAL
        f = open(filename, "r")

        for line in f:
            line = line.rstrip('\n')
            ArangoshSetup += line + "\n"

        f.close()
    
    ## .............................................................................
    ## output directory
    ## .............................................................................

    elif fstate == STATE_OUTPUT_DIR:
        fstate = STATE_NORMAL
        OutputDir = filename

################################################################################
### @brief generate single line arangosh examples
################################################################################

def generateExampleArangosh():
    print "var internal = require('internal');"
    print "var ArangoshExamples = {};"
    print "internal.startPrettyPrint(true);"
    print "internal.stopColorPrint(true);"

    print
    print "(function () {\n%s}());" % ArangoshSetup
    print
    
    for key in ArangoshExamples:
        value = ArangoshExamples[key]

        print "(function() {"
        print "internal.startCaptureMode();";
        print "try { var value = %s; print(value); } catch (err) { print(err); }" % value
        print "var output = internal.stopCaptureMode();";
        print "ArangoshExamples['%s'] = output;" % key
        print "}());"

    print

    for key in ArangoshExamples:
        print "internal.write('%s/%s.generated', ArangoshExamples['%s']);" % (OutputDir, key, key)

generateExampleArangosh()
