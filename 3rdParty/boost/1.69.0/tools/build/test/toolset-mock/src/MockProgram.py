# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import sys
import os
import re

# Represents a sequence of arguments that must appear
# in a fixed order.
class ordered:
    def __init__(self, *args):
        self.args = args
    def match(self, command_line, pos, outputs):
        for p in self.args:
            res = try_match(command_line, pos, p, outputs)
            if res is None:
                return
            pos = res
        return pos

# Represents a sequence of arguments that can appear
# in any order.
class unordered:
    def __init__(self, *args):
        self.args = list(args)
    def match(self, command_line, pos, outputs):
        unmatched = self.args[:]
        while len(unmatched) > 0:
            res = try_match_one(command_line, pos, unmatched, outputs)
            if res is None:
                return
            pos = res
        return pos

# Represents a single input file.
# If id is set, then the file must have been created
# by a prior use of output_file.
# If source is set, then the file must be that source file.
class input_file:
    def __init__(self, id=None, source=None):
        assert((id is None) ^ (source is None))
        self.id = id
        self.source = source
    def check(self, path):
        if path.startswith("-"):
            return
        if self.id is not None:
            try:
                with open(path, "r") as f:
                    data = f.read()
                    if data == make_file_contents(self.id):
                        return True
                    else:
                        return
            except:
                return
        elif self.source is not None:
            if self.source == path:
                return True
            else:
                return
        assert(False)
    def match(self, command_line, pos, outputs):
        if self.check(command_line[pos]):
            return pos + 1

# Matches an output file.
# If the full pattern is matched, The
# file will be created.
class output_file:
    def __init__(self, id):
        self.id = id
    def match(self, command_line, pos, outputs):
        if command_line[pos].startswith("-"):
            return
        outputs.append((command_line[pos], self.id))
        return pos + 1

# Matches the directory containing an input_file
class target_path(object):
    def __init__(self, id):
        self.tester = input_file(id=id)
    def match(self, command_line, pos, outputs):
        arg = command_line[pos]
        if arg.startswith("-"):
            return
        try:
            for path in os.listdir(arg):
                if self.tester.check(os.path.join(arg, path)):
                    return pos + 1
        except:
            return                 

# Matches a single argument, which is composed of a prefix and a path
# for example arguments of the form -ofilename.
class arg(object):
    def __init__(self, prefix, a):
        # The prefix should be a string, a should be target_path or input_file.
        self.prefix = prefix
        self.a = a
    def match(self, command_line, pos, outputs):
        s = command_line[pos]
        if s.startswith(self.prefix) and try_match([s[len(self.prefix):]], 0, self.a, outputs) == 1:
            return pos + 1

# Given a file id, returns a string that will be
# written to the file to allow it to be recognized.
def make_file_contents(id):
    return id

# Matches a single pattern from a list.
# If it succeeds, the matching pattern
# is removed from the list.
# Returns the index after the end of the match
def try_match_one(command_line, pos, patterns, outputs):
    for p in patterns:
        tmp = outputs[:]
        res = try_match(command_line, pos, p, tmp)
        if res is not None:
            outputs[:] = tmp
            patterns.remove(p)
            return res

# returns the end of the match if any
def try_match(command_line, pos, pattern, outputs):
    if pos == len(command_line):
        return
    elif type(pattern) is str:
        if pattern == command_line[pos]:
            return pos + 1
    else:
        return pattern.match(command_line, pos, outputs)

known_patterns = []
program_name = None

# Registers a command
# The arguments should be a sequence of:
# str, ordered, unordered, arg, input_file, output_file, target_path
# kwarg: stdout is text that will be printed on success.
def command(*args, **kwargs):
    global known_patterns
    global program_name
    stdout = kwargs.get("stdout", None)
    pattern = ordered(*args)
    known_patterns += [(pattern, stdout)]
    if program_name is None:
        program_name = args[0]
    else:
        assert(program_name == args[0])

# Use this to filter the recognized commands, based on the properties
# passed to b2.
def allow_properties(*args):
    try:
        return all(a in os.environ["B2_PROPERTIES"].split(" ") for a in args)
    except KeyError:
        return True

# Use this in the stdout argument of command to print the command
# for running another script.
def script(name):
    return os.path.join(os.path.dirname(__file__), "bin", re.sub('\.py$', '', name))

def match(command_line):
    for (p, stdout) in known_patterns:
        outputs = []
        if try_match(command_line, 0, p, outputs) == len(command_line):
            return (stdout, outputs)

# Every mock program should call this after setting up all the commands.
def main():
    command_line = [program_name] + sys.argv[1:]
    result = match(command_line)
    if result is not None:
        (stdout, outputs) = result
        if stdout is not None:
            print stdout
        for (file,id) in outputs:
            with open(file, "w") as f:
                f.write(make_file_contents(id))
        exit(0)
    else:
        print command_line
        exit(1)

# file should be the name of a file in the same directory
# as this.  Must be called after verify_setup
def verify_file(filename):
    global known_files
    if filename not in known_files:
        known_files.add(filename)
        srcdir = os.path.dirname(__file__)
        execfile(os.path.join(srcdir, filename), {})

def verify_setup():
    """Override the behavior of most module components
    in order to detect whether they are being used correctly."""
    global main
    global allow_properties
    global output_file
    global input_file
    global target_path
    global script
    global command
    global verify_errors
    global output_ids
    global input_ids
    global known_files
    def allow_properties(*args):
        return True
    def main():
        pass
    def output_file(id):
        global output_ids
        global verify_error
        if id in output_ids:
            verify_error("duplicate output_file: %s" % id)
        output_ids.add(id)
    def input_file(id=None, source=None):
        if id is not None:
            input_ids.add(id)
    def target_path(id):
        input_ids.add(id)
    def script(filename):
        verify_file(filename)
    def command(*args, **kwargs):
        pass
    verify_errors = []
    output_ids = set()
    input_ids = set()
    known_files = set()

def verify_error(message):
    global verify_errors
    verify_errors += [message]

def verify_finalize():
    for id in input_ids:
        if not id in output_ids:
            verify_error("Input file does not exist: %s" % id)
    for error in verify_errors:
        print "error: %s" % error
    if len(verify_errors) != 0:
        return 1
    else:
        return 0

def verify():
    srcdir = os.path.dirname(__file__)
    if srcdir == '':
        srcdir = '.'
    verify_setup()
    for f in os.listdir(srcdir):
        if re.match(r"(gcc|clang|darwin|intel)-.*\.py", f):
            verify_file(f)
    exit(verify_finalize())
