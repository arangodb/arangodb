"""

	Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.
	Copyright (c) Sorin Fetche

	Distributed under the Boost Software License, Version 1.0. (See accompanying
	file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

	This program generates a single header file from a file including multiple C/C++ headers.

	Usage:

		python3 generate_single_header.py  --help

		e.g. python3 generate_single_header.py -i include/boost/qvm/all.hpp -p include -o test/qvm.hpp boost/qvm

"""

import argparse
import os
import re
from datetime import date
import subprocess

included = []

def append(input_file_name, input_file, output_file, regex_includes, include_folder):
	line_count = 1
	last_line_was_empty = False
	for line in input_file:
		line_count += 1
		this_line_is_empty = (line=='\n')
		result = regex_includes.search(line)
		if result:
			next_input_file_name = result.group("include")
			if next_input_file_name not in included:
				included.append(next_input_file_name)
				print("%s" % next_input_file_name)
				with open(os.path.join(include_folder, next_input_file_name), "r") as next_input_file:
					output_file.write('// >>> %s#line 1 "%s"\n' % (line, next_input_file_name))
					append(next_input_file_name, next_input_file, output_file, regex_includes, include_folder)
					output_file.write('// <<< %s#line %d "%s"\n' % (line, line_count, input_file_name))
		else:
			if '///' in line:
				continue
			if this_line_is_empty and last_line_was_empty:
				continue
			output_file.write(line)
		last_line_was_empty = this_line_is_empty

def _main():
	parser = argparse.ArgumentParser(
		description="Generates a single include file from a file including multiple C/C++ headers")
	parser.add_argument("-i", "--input", action="store", type=str, default="in.cpp",
		help="Input file including the headers to be merged")
	parser.add_argument("-o", "--output", action="store", type=str, default="out.cpp",
		help="Output file. NOTE: It will be overwritten!")
	parser.add_argument("-p", "--path", action="store", type=str, default=".",
		help="Include path")
	parser.add_argument("prefix", action="store", type=str,
		help="Non-empty include file prefix (e.g. a/b)")
	args = parser.parse_args()

	regex_includes = re.compile(r"""^\s*#[\t\s]*include[\t\s]*("|\<)(?P<include>%s.*)("|\>)""" % args.prefix)
	print("Rebuilding %s:" % args.input)
	with open(args.output, 'w') as output_file, open(args.input, 'r') as input_file:
		output_file.write(
			'// QVM single header distribution. Do not edit.\n'
			'\n'
			'// Generated from https://github.com/boostorg/qvm on ' + date.today().strftime("%B %d, %Y") + ',\n'
			'// Git hash ' + subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode().rstrip() + '.\n'
			'\n'
			'// Latest versions:\n'
			'//      https://boostorg.github.io/qvm/qvm.hpp\n'
			'//      https://boostorg.github.io/qvm/qvm_lite.hpp\n'
			'\n'
			'// Copyright (c) 2008-2021 Emil Dotchevski and Reverge Studios, Inc.\n'
			'\n'
			'// Distributed under the Boost Software License, Version 1.0. (See accompanying\n'
			'// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)\n'
			'\n' )
		append(args.input, input_file, output_file, regex_includes, args.path)

if __name__ == "__main__":
	_main()
