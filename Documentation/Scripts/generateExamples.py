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
### @brief enable debug output in JavaScript
################################################################################

JS_DEBUG = False

################################################################################
### @brief output directory
################################################################################

OutputDir = "/tmp/"

################################################################################
### @brief arangosh output
################################################################################

ArangoshOutput = {}

################################################################################
### @brief arangosh run
################################################################################

ArangoshRun = {}

################################################################################
### @brief arangosh output files
################################################################################

ArangoshFiles = {}

################################################################################
### @brief global setup for arangosh
################################################################################

ArangoshSetup = ""

################################################################################
### @brief states
################################################################################

STATE_BEGIN = 0
STATE_ARANGOSH_OUTPUT = 1
STATE_ARANGOSH_RUN = 2

state = STATE_BEGIN

################################################################################
### @brief parse file
################################################################################

r1 = re.compile(r'^(/// )?@EXAMPLE_ARANGOSH_OUTPUT{([^}]*)}')
r2 = re.compile(r'^(/// )?@END_EXAMPLE_ARANGOSH_OUTPUT')
r3 = re.compile(r'^(/// )?@EXAMPLE_ARANGOSH_RUN{([^}]*)}')
r4 = re.compile(r'^(/// )?@END_EXAMPLE_ARANGOSH_RUN')

name = ""

OPTION_NORMAL = 0
OPTION_ARANGOSH_SETUP = 1
OPTION_OUTPUT_DIR = 2

fstate = OPTION_NORMAL
strip = None

for filename in argv:
    if filename == "--arangosh-setup":
        fstate = OPTION_ARANGOSH_SETUP
        continue

    if filename == "--output-dir":
        fstate = OPTION_OUTPUT_DIR
        continue

    ## .............................................................................
    ## input file
    ## .............................................................................
    
    if fstate == OPTION_NORMAL:
        f = open(filename, "r")
        state = STATE_BEGIN

        for line in f:
            if strip is None:
                strip = ""

            line = line.rstrip('\n')

            if state == STATE_BEGIN:
                m = r1.match(line)

                if m:
                    strip = m.group(1)
                    name = m.group(2)

                    if name in ArangoshFiles:
                        print >> sys.stderr, "%s\nduplicate file name '%s'\n%s\n" % ('#' * 80, name, '#' * 80)
                        
                    ArangoshFiles[name] = True
                    ArangoshOutput[name] = ""
                    state = STATE_ARANGOSH_OUTPUT
                    continue

                m = r3.match(line)

                if m:
                    strip = m.group(1)
                    name = m.group(2)

                    if name in ArangoshFiles:
                        print >> sys.stderr, "%s\nduplicate file name '%s'\n%s\n" % ('#' * 80, name, '#' * 80)
                        
                    ArangoshFiles[name] = True
                    ArangoshRun[name] = ""
                    state = STATE_ARANGOSH_RUN
                    continue

            elif state == STATE_ARANGOSH_OUTPUT:
                m = r2.match(line)

                if m:
                    name = ""
                    state = STATE_BEGIN
                    continue

                line = line[len(strip):]

                ArangoshOutput[name] += line + "\n"

            elif state == STATE_ARANGOSH_RUN:
                m = r4.match(line)

                if m:
                    name = ""
                    state = STATE_BEGIN
                    continue

                line = line[len(strip):]

                ArangoshRun[name] += line + "\n"

        f.close()

    ## .............................................................................
    ## arangosh setup file
    ## .............................................................................

    elif fstate == OPTION_ARANGOSH_SETUP:
        fstate = OPTION_NORMAL
        f = open(filename, "r")

        for line in f:
            line = line.rstrip('\n')
            ArangoshSetup += line + "\n"

        f.close()
    
    ## .............................................................................
    ## output directory
    ## .............................................................................

    elif fstate == OPTION_OUTPUT_DIR:
        fstate = OPTION_NORMAL
        OutputDir = filename

################################################################################
### @brief generate arangosh example
################################################################################

def generateArangoshOutput():
    print "var internal = require('internal');"
    print "var ArangoshOutput = {};"
    print "internal.startPrettyPrint(true);"
    print "internal.stopColorPrint(true);"

    if JS_DEBUG:
        print "internal.output('%s\\n');" % ('=' * 80)
        print "internal.output('ARANGOSH EXAMPLE\\n');"
        print "internal.output('%s\\n');" % ('=' * 80)

    print
    print "(function () {\n%s}());" % ArangoshSetup
    print
    
    for key in ArangoshOutput:
        value = ArangoshOutput[key]

        print "(function() {"
        print "internal.startCaptureMode();";
        print "try { var value = %s; print(value); } catch (err) { print(err); }" % value
        print "var output = internal.stopCaptureMode();";
        print "ArangoshOutput['%s'] = output;" % key
        if JS_DEBUG:
            print "internal.output('%s', ':\\n', output, '\\n%s\\n');" % (key, '-' * 80)
        print "}());"

    for key in ArangoshOutput:
        print "internal.write('%s/%s.generated', ArangoshOutput['%s']);" % (OutputDir, key, key)

################################################################################
### @brief generate arangosh run
################################################################################

def generateArangoshRun():
    print "var internal = require('internal');"
    print "var ArangoshRun = {};"
    print "internal.startPrettyPrint(true);"
    print "internal.stopColorPrint(true);"

    if JS_DEBUG:
        print "internal.output('%s\\n');" % ('=' * 80)
        print "internal.output('ARANGOSH RUN\\n');"
        print "internal.output('%s\\n');" % ('=' * 80)

    print
    print "(function () {\n%s}());" % ArangoshSetup
    print
    
    for key in ArangoshRun:
        value = ArangoshRun[key]

        print "(function() {"
        print "internal.output('\\n%s\\nRUN STARTING: %s\\n%s\\n');" % ('#' * 80, key, '#' * 80)
        print "var output = '';"
        print "var appender = function(text) { output += text; };"
        print "var logCurlRequestRaw = require('internal').appendCurlRequest(appender);"
        print "var logCurlRequest = function () { var r = logCurlRequestRaw.apply(logCurlRequestRaw, arguments); db._collections(); return r; };"
        print "var logJsonResponse = require('internal').appendJsonResponse(appender);"
        print "var assert = function(a) { if (! a) { internal.output('%s\\nASSERTION FAILED: %s\\n%s\\n'); } };" % ('#' * 80, key, '#' * 80)
        print "try { %s internal.output('\\n%s\\nRUN SUCCEEDED: %s\\n%s\\n'); } catch (err) { print('%s\\nRUN FAILED: %s, ', err, '\\n%s\\n'); }" % (value, '#' * 80, key, '#' * 80, '#' * 80, key, '#' * 80)
        print "ArangoshRun['%s'] = output;" % key
        if JS_DEBUG:
            print "internal.output('%s', ':\\n', output, '\\n%s\\n');" % (key, '-' * 80)
        print "}());"

    for key in ArangoshRun:
        print "internal.write('%s/%s.generated', ArangoshRun['%s']);" % (OutputDir, key, key)

################################################################################
### @brief main
################################################################################

generateArangoshOutput()
generateArangoshRun()
