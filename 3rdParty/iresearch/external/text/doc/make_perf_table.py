#!/usr/bin/env python

# Copyright (C) 2020 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import argparse
import json

parser = argparse.ArgumentParser(description='Takes a Google Benchmark JSON output file and multiple test names, and produces a QuickBook table.')
parser.add_argument('json_file', type=str, help='The name of the JSON file to process.')
parser.add_argument('test_names', nargs='+', type=str, help='The names of the tests to process.  Append a comma and a second name to each entry to rename; all or none of the names must be this way.  E.g., "foo,bar baz,quux"')
args = parser.parse_args()

json_contents = json.load(open(args.json_file))

def has_comma(s):
    return ',' in s

comma_in_first_name = has_comma(args.test_names[0])
for name in args.test_names:
    if has_comma(name) != comma_in_first_name:
        raise Exception('All or none of the test names must have commas-renames.')

if comma_in_first_name:
    test_names = []
    test_renames = {}
    for both_names in args.test_names:
        test_name, test_rename = both_names.split(',')
        test_names.append(test_name)
        test_renames[test_name] = test_rename
else:
    test_names = args.test_names
    test_renames = dict(map(lambda x: (x, x), args.test_names))

# name -> { N<int> -> (time<float>, time_with_unit<string>) }
aggregated_results = {}

for b in json_contents['benchmarks']:
    full_name = b['name']
    if '/' in full_name:
        name, n = full_name.split('/')
    else:
        name = full_name
        n = 1
    if name in test_names:
        if name not in aggregated_results:
            aggregated_results[name] = {}
        result = (int(b['cpu_time']) * 1.0, '{} {}'.format(b['cpu_time'], b['time_unit']))
        aggregated_results[name][int(n)] = result
#        print b

title_row = '[N] '
title_row += ''.join(map(lambda x: '[{}] '.format(test_renames[x]), test_names))
if len(test_names) == 2:
    title_row  += '[{} / {}] '.format(test_renames[test_names[0]], test_renames[test_names[1]])

n_values_per_name = map(lambda x: aggregated_results[x].keys(), test_names)
same_results_as_first = map(lambda x: x == n_values_per_name[0], n_values_per_name)

if not all(same_results_as_first):
    print 'N values:'
    print '   ',  n_values_per_name
    raise Exception('Cannot generate table for perf tests with a different number of results, or different values of N.')

data_rows = ''
for n in sorted(n_values_per_name[0]):
    data_rows += '    [ '
    data_rows += '[{}] '.format(n)
    for name in test_names:
        data_rows += '[{}] '.format(aggregated_results[name][n][1])
    if len(test_names) == 2:
        data_rows += '[{:.3}] '.format(aggregated_results[test_names[0]][n][0] / aggregated_results[test_names[1]][n][0])
    data_rows += ']\n'

print '''\
[table TITLE
    [ {}]
{}]
'''.format(title_row, data_rows)
