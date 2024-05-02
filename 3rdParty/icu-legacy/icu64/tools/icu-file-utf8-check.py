#! /usr/bin/python -B

# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# Copyright (C) 2009-2011, International Business Machines Corporation, Google and Others.
# All rights reserved.

#
#  Script to check that ICU source files contain only valid UTF-8 encoded text,
#  and that all files except '.txt' files do not contain a Byte Order Mark (BOM).
#
#  THIS SCRIPT DOES NOT WORK ON WINDOWS
#     It only works correctly on platforms where the native line ending is a plain \n
#
#  usage:
#     icu-file-utf8-check.py  [options]
#
#  options:
#     -h | --help    Print a usage line and exit.
#
#  The tool operates recursively on the directory from which it is run.
#  Only files from the ICU github repository are checked.
#  No changes are made to the repository; only the working copy will be altered.

from __future__ import print_function

import sys
import os
import os.path
import re
import getopt


def runCommand(cmd):
    output_file = os.popen(cmd);
    output_text = output_file.read();
    exit_status = output_file.close();
    if exit_status:
        print('"', cmd, '" failed.  Exiting.', file=sys.stderr)
        sys.exit(exit_status)
    return output_text


def usage():
    print("usage: " + sys.argv[0] + " [-h | --help]")

    
#
#  File check.         Check source code files for UTF-8 and all except text files for not containing a BOM
#    file_name:        name of a text file.
#    is_source:        Flag, set to True if file is a source code file (.c, .cpp, .h, .java).
#
def check_file(file_name, is_source):
    f = open(file_name, 'rb')
    bytes = f.read()
    f.close()

    if is_source:
        try:
            bytes.decode("UTF-8")
        except UnicodeDecodeError:
            print("Error: %s is a source code file but contains non-utf-8 bytes." % file_name)
    
    if bytes[0] == 0xef:
        if not (file_name.endswith(".txt") or file_name.endswith(".sln")
                    or file_name.endswith(".targets")
                    or ".vcxproj" in file_name):
            print("Warning: file %s contains a UTF-8 BOM: " % file_name)

    return

def main(argv):
    try:
        opts, args = getopt.getopt(argv, "h", ("help"))
    except getopt.GetoptError:
        print("unrecognized option: " + argv[0])
        usage()
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
            sys.exit()
    if args:
        print("unexpected command line argument")
        usage()
        sys.exit()

    output = runCommand("git ls-files ");
    file_list = output.splitlines()

    source_file_re = re.compile(".*((?:\\.c$)|(?:\\.cpp$)|(?:\\.h$)|(?:\\.java$))")
    
    for f in file_list:
        if os.path.isdir(f):
            print("Skipping dir " + f)
            continue
        if not os.path.isfile(f):
            print("Repository file not in working copy: " + f)
            continue;

        source_file = source_file_re.match(f)
        check_file(f, source_file)

if __name__ == "__main__":
    main(sys.argv[1:])
