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
parser.add_argument('ns_scale', type=int, help='The number of nanoseconds that represent full with in the chart.')
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

chart_width = 500
bar_spacing = 25
max_bar_width = 300
bar_height = 20

colors = [
    '#32a852', '#3f6078', '#ad3939', '#ad9e39', '#ba45cc'
]

max_value = args.ns_scale * 1.0

print '''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="{}" height="{}">'''.format(
    chart_width,
    (len(test_names) + 1) * bar_spacing # +1 for chart x-axis
)

bar_y = 0
color_index = 0
for n in sorted(n_values_per_name[0]):
    for name in test_names:
        if len(colors) <= color_index:
            print 'Out of colors!'
            exit(1)
        this_bar_width = aggregated_results[name][n][0] / max_value * max_bar_width
        print '''    <rect width="{1}" height="{2}" y="{3}" fill="{0}"></rect>
    <text x="{4}" y="{5}" dy=".35em" fill="black">{6}</text>'''.format(
      colors[color_index],
      this_bar_width,
      bar_height,
      bar_y,
      this_bar_width + 5,
      bar_y + bar_height / 2.0,
      name in test_renames and test_renames[name] or name
  )
        bar_y += bar_spacing
        color_index += 1

print '''    <rect width="{0}" height="{1}" y="{2}" fill="white"></rect>'''.format(
    max_bar_width,
    bar_height,
    bar_y
)

bar_y += 4# - bar_spacing
text_y = bar_y + bar_height * 0.75
print '''    <line x1="1" y1="{0}" x2="{1}" y2="{0}" stroke="black" fill="black"></line>
    <line x1="1" y1="{2}" x2="1" y2="{3}" stroke="black" fill="black"></line>
    <line x1="{1}" y1="{2}" x2="{1}" y2="{3}" stroke="black" fill="black"></line>
    <text x="0" y="{5}" dy=".35em" fill="black">0 ns</text>
    <text x="{1}" y="{5}" dy=".35em" text-anchor="end" fill="black">{4} ns</text>'''.format(
    bar_y,
    max_bar_width,
    bar_y - 5,
    bar_y + 5,
    args.ns_scale,
    text_y
)

print '</svg>'
