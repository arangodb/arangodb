#!/bin/env python3
""" test the syntax of the test-definition.txt and output the results """
def generate_dump_output(_, tests):
    """ interactive version output to inspect comprehension """
    def output(line):
        """ output one line """
        print(line)

    for test in tests:
        params = " ".join(f"{key}={value}" for key, value in test['params'].items())
        output(f"{test['name']}")
        output(f"\tpriority: {test['priority']}")
        output(f"\tparallelity: {test['parallelity']}")
        output(f"\tflags: {' '.join(test['flags'])}")
        output(f"\tparams: {params}")
        output(f"\targs: {' '.join(test['args'])}")
