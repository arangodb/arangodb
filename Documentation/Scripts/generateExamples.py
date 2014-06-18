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
### @author Copyright 2011-2014, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, string, os

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
###
### A list of commands that are executed in order to produce the output. The
### commands and there output is logged.
################################################################################

ArangoshOutput = {}

################################################################################
### @brief arangosh run
###
### A list of commands that are executed in order to produce the output. This
### is mostly used for HTTP request examples.
################################################################################

ArangoshRun = {}

################################################################################
### @brief arangosh output files
################################################################################

ArangoshFiles = {}

################################################################################
### @brief arangosh examples, in some deterministic order
################################################################################

ArangoshCases = [ ]

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

OPTION_NORMAL = 0
OPTION_ARANGOSH_SETUP = 1
OPTION_OUTPUT_DIR = 2

fstate = OPTION_NORMAL

filenames = []

for filename in argv:
    if filename == "--arangosh-setup":
        fstate = OPTION_ARANGOSH_SETUP
        continue

    if filename == "--output-dir":
        fstate = OPTION_OUTPUT_DIR
        continue

    if fstate == OPTION_NORMAL:
        if os.path.isdir(filename):
            for root, dirs, files in os.walk(filename):
                for file in files:
                    if file.endswith(".mdpp") or file.endswith(".js") or file.endswith(".cpp"):
                        filenames.append(os.path.join(root, file))
        else:
            filenames.append(filename)

    elif fstate == OPTION_ARANGOSH_SETUP:
        fstate = OPTION_NORMAL
        f = open(filename, "r")

        for line in f:
            line = line.rstrip('\n')
            ArangoshSetup += line + "\n"

        f.close()
    
    elif fstate == OPTION_OUTPUT_DIR:
        fstate = OPTION_NORMAL
        OutputDir = filename

## .............................................................................
## loop over input files
## .............................................................................
    
r1 = re.compile(r'^(/// )?@EXAMPLE_ARANGOSH_OUTPUT{([^}]*)}')
r2 = re.compile(r'^(/// )?@EXAMPLE_ARANGOSH_RUN{([^}]*)}')
r3 = re.compile(r'^@END_EXAMPLE_')

name = ""

fstate = OPTION_NORMAL
strip = None

partialCmd = ""
partialLine = ""

for filename in filenames:

    f = open(filename, "r")
    state = STATE_BEGIN

    for line in f:
        if strip is None:
            strip = ""

        line = line.rstrip('\n')

        # read the start line and remember the prefix which must be skipped

        if state == STATE_BEGIN:
            m = r1.match(line)

            if m:
                strip = m.group(1)
                name = m.group(2)

                if name in ArangoshFiles:
                    print >> sys.stderr, "%s\nduplicate file name '%s'\n%s\n" % ('#' * 80, name, '#' * 80)

                ArangoshFiles[name] = True
                ArangoshOutput[name] = []
                state = STATE_ARANGOSH_OUTPUT
                continue

            m = r2.match(line)

            if m:
                strip = m.group(1)
                name = m.group(2)

                if name in ArangoshFiles:
                    print >> sys.stderr, "%s\nduplicate file name '%s'\n%s\n" % ('#' * 80, name, '#' * 80)

                ArangoshCases.append(name)    
                ArangoshFiles[name] = True
                ArangoshRun[name] = ""
                state = STATE_ARANGOSH_RUN
                continue

            continue

        # we are within a example handle any continued line magic
        line = line[len(strip):]
        m = r3.match(line)
        showCmd = True

        if m:
            name = ""
            partialLine = ""
            partialCmd = ""
            state = STATE_BEGIN
            continue

        cmd = line.replace("\\", "\\\\").replace("'", "\\'")

        if line != "":
            if line[0] == "|":
                partialLine = partialLine + line[1:] + "\n"
                cmd = cmd[1:]

                if partialCmd == "":
                    partialCmd = "arangosh> " + cmd + "\\n"
                else:
                    partialCmd = partialCmd + "........> " + cmd + "\\n"

                continue

            if line[0] == "~":
                line = line[1:]
                showCmd = False

        line = partialLine + line
        partialLine = ""

        if showCmd:
            if partialCmd == "":
                cmd = "arangosh> " + cmd
            else:
                cmd = partialCmd + "........> " + cmd

            partialCmd = ""
        else:
            cmd = None

        if state == STATE_ARANGOSH_OUTPUT:
            ArangoshOutput[name].append([line, cmd])

        elif state == STATE_ARANGOSH_RUN:
            ArangoshRun[name] += line + "\n"

    f.close()

################################################################################
### @brief generate arangosh example
################################################################################

gr1 = re.compile(r'^[ \n]*(while|if|var) ')

def generateArangoshOutput():
    print "var internal = require('internal');"
    print "var fs = require('fs');"
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
        print "try {"
        print "  var XXX;"

        for l in value:
            m = gr1.match(l[0])

            if l[1]:
                print "print('%s');" % l[1]

            if m:
                print "%s" % l[0]
            else:
                print "XXX = %s" % l[0]
                print "print(XXX);"

        print "} catch (err) { print(err); }"
        print "var output = internal.stopCaptureMode();";
        print "ArangoshOutput['%s'] = output;" % key
        if JS_DEBUG:
            print "internal.output('%s', ':\\n', output, '\\n%s\\n');" % (key, '-' * 80)
        print "}());"

    for key in ArangoshOutput:
        print "fs.write('%s/%s.generated', ArangoshOutput['%s']);" % (OutputDir, key, key)

################################################################################
### @brief generate arangosh run
################################################################################

def generateArangoshRun():
    print "var internal = require('internal');"
    print "var fs = require('fs');"
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
    
    for key in ArangoshCases:
        value = ArangoshRun[key]

        print "(function() {"
        print "internal.output('RUN STARTING:  %s\\n');" % key
        print "var output = '';"
        print "var appender = function(text) { output += text; };"
        print "var log = function (a) { internal.startCaptureMode(); print(a); appender(internal.stopCaptureMode()); };"
        print "var logCurlRequestRaw = internal.appendCurlRequest(appender);"
        print "var logCurlRequest = function () { var r = logCurlRequestRaw.apply(logCurlRequestRaw, arguments); db._collections(); return r; };"
        print "var curlRequestRaw = internal.appendCurlRequest(function (text) {});"
        print "var curlRequest = function () { return curlRequestRaw.apply(curlRequestRaw, arguments); };"
        print "var logJsonResponse = internal.appendJsonResponse(appender);"
        print "var logRawResponse = internal.appendRawResponse(appender);"
        print "var assert = function(a) { if (! a) { internal.output('%s\\nASSERTION FAILED: %s\\n%s\\n'); throw new Error('assertion failed'); } };" % ('#' * 80, key, '#' * 80)
        print "try { %s internal.output('RUN SUCCEEDED: %s\\n'); } catch (err) { print('%s\\nRUN FAILED: %s, ', err, '\\n%s\\n'); }" % (value, key, '#' * 80, key, '#' * 80)
        print "ArangoshRun['%s'] = output;" % key
        if JS_DEBUG:
            print "internal.output('%s', ':\\n', output, '\\n%s\\n');" % (key, '-' * 80)
        print "fs.write('%s/%s.generated', ArangoshRun['%s']);" % (OutputDir, key, key)
        print "}());"

################################################################################
### @brief main
################################################################################

generateArangoshOutput()
generateArangoshRun()
