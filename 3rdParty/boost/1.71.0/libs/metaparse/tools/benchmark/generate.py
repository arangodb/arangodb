#!/usr/bin/python
"""Utility to generate files to benchmark"""

# Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

import argparse
import os
import string
import random
import re
import json

import Cheetah.Template
import chars


def regex_to_error_msg(regex):
    """Format a human-readable error message from a regex"""
    return re.sub('([^\\\\])[()]', '\\1', regex) \
        .replace('[ \t]*$', '') \
        .replace('^', '') \
        .replace('$', '') \
        .replace('[ \t]*', ' ') \
        .replace('[ \t]+', ' ') \
        .replace('[0-9]+', 'X') \
        \
        .replace('\\[', '[') \
        .replace('\\]', ']') \
        .replace('\\(', '(') \
        .replace('\\)', ')') \
        .replace('\\.', '.')


def mkdir_p(path):
    """mkdir -p path"""
    try:
        os.makedirs(path)
    except OSError:
        pass


def in_comment(regex):
    """Builds a regex matching "regex" in a comment"""
    return '^[ \t]*//[ \t]*' + regex + '[ \t]*$'


def random_chars(number):
    """Generate random characters"""
    char_map = {
        k: v for k, v in chars.CHARS.iteritems()
        if not format_character(k).startswith('\\x')
    }

    char_num = sum(char_map.values())
    return (
        format_character(nth_char(char_map, random.randint(0, char_num - 1)))
        for _ in xrange(0, number)
    )


def random_string(length):
    """Generate a random string or character list depending on the mode"""
    return \
        'BOOST_METAPARSE_STRING("{0}")'.format(''.join(random_chars(length)))


class Mode(object):
    """Represents a generation mode"""

    def __init__(self, name):
        self.name = name
        if name == 'BOOST_METAPARSE_STRING':
            self.identifier = 'bmp'
        elif name == 'manual':
            self.identifier = 'man'
        else:
            raise Exception('Invalid mode: {0}'.format(name))

    def description(self):
        """The description of the mode"""
        if self.identifier == 'bmp':
            return 'Using BOOST_METAPARSE_STRING'
        elif self.identifier == 'man':
            return 'Generating strings manually'

    def convert_from(self, base):
        """Convert a BOOST_METAPARSE_STRING mode document into one with
        this mode"""
        if self.identifier == 'bmp':
            return base
        elif self.identifier == 'man':
            result = []
            prefix = 'BOOST_METAPARSE_STRING("'
            while True:
                bmp_at = base.find(prefix)
                if bmp_at == -1:
                    return ''.join(result) + base
                else:
                    result.append(
                        base[0:bmp_at] + '::boost::metaparse::string<'
                    )
                    new_base = ''
                    was_backslash = False
                    comma = ''
                    for i in xrange(bmp_at + len(prefix), len(base)):
                        if was_backslash:
                            result.append(
                                '{0}\'\\{1}\''.format(comma, base[i])
                            )
                            was_backslash = False
                            comma = ','
                        elif base[i] == '"':
                            new_base = base[i+2:]
                            break
                        elif base[i] == '\\':
                            was_backslash = True
                        else:
                            result.append('{0}\'{1}\''.format(comma, base[i]))
                            comma = ','
                    base = new_base
                    result.append('>')


class Template(object):
    """Represents a loaded template"""

    def __init__(self, name, content):
        self.name = name
        self.content = content

    def instantiate(self, value_of_n):
        """Instantiates the template"""
        template = Cheetah.Template.Template(
            self.content,
            searchList={'n': value_of_n}
        )
        template.random_string = random_string
        return str(template)

    def range(self):
        """Returns the range for N"""
        match = self._match(in_comment(
            'n[ \t]+in[ \t]*\\[([0-9]+)\\.\\.([0-9]+)\\),[ \t]+'
            'step[ \t]+([0-9]+)'
        ))
        return range(
            int(match.group(1)),
            int(match.group(2)),
            int(match.group(3))
        )

    def property(self, name):
        """Parses and returns a property"""
        return self._get_line(in_comment(name + ':[ \t]*(.*)'))

    def modes(self):
        """Returns the list of generation modes"""
        return [Mode(s.strip()) for s in self.property('modes').split(',')]

    def _match(self, regex):
        """Find the first line matching regex and return the match object"""
        cregex = re.compile(regex)
        for line in self.content.splitlines():
            match = cregex.match(line)
            if match:
                return match
        raise Exception('No "{0}" line in {1}.cpp'.format(
            regex_to_error_msg(regex),
            self.name
        ))

    def _get_line(self, regex):
        """Get a line based on a regex"""
        return self._match(regex).group(1)


def load_file(path):
    """Returns the content of the file"""
    with open(path, 'rb') as in_file:
        return in_file.read()


def templates_in(path):
    """Enumerate the templates found in path"""
    ext = '.cpp'
    return (
        Template(f[0:-len(ext)], load_file(os.path.join(path, f)))
        for f in os.listdir(path) if f.endswith(ext)
    )


def nth_char(char_map, index):
    """Returns the nth character of a character->occurrence map"""
    for char in char_map:
        if index < char_map[char]:
            return char
        index = index - char_map[char]
    return None


def format_character(char):
    """Returns the C-formatting of the character"""
    if \
        char in string.ascii_letters \
        or char in string.digits \
        or char in [
                '_', '.', ':', ';', ' ', '!', '?', '+', '-', '/', '=', '<',
                '>', '$', '(', ')', '@', '~', '`', '|', '#', '[', ']', '{',
                '}', '&', '*', '^', '%']:
        return char
    elif char in ['"', '\'', '\\']:
        return '\\{0}'.format(char)
    elif char == '\n':
        return '\\n'
    elif char == '\r':
        return '\\r'
    elif char == '\t':
        return '\\t'
    else:
        return '\\x{:02x}'.format(ord(char))


def write_file(filename, content):
    """Create the file with the given content"""
    print 'Generating {0}'.format(filename)
    with open(filename, 'wb') as out_f:
        out_f.write(content)


def out_filename(template, n_val, mode):
    """Determine the output filename"""
    return '{0}_{1}_{2}.cpp'.format(template.name, n_val, mode.identifier)


def main():
    """The main function of the script"""
    desc = 'Generate files to benchmark'
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument(
        '--src',
        dest='src_dir',
        default='src',
        help='The directory containing the templates'
    )
    parser.add_argument(
        '--out',
        dest='out_dir',
        default='generated',
        help='The output directory'
    )
    parser.add_argument(
        '--seed',
        dest='seed',
        default='13',
        help='The random seed (to ensure consistent regeneration)'
    )

    args = parser.parse_args()

    random.seed(int(args.seed))

    mkdir_p(args.out_dir)

    for template in templates_in(args.src_dir):
        modes = template.modes()

        n_range = template.range()
        for n_value in n_range:
            base = template.instantiate(n_value)
            for mode in modes:
                write_file(
                    os.path.join(
                        args.out_dir,
                        out_filename(template, n_value, mode)
                    ),
                    mode.convert_from(base)
                )
        write_file(
            os.path.join(args.out_dir, '{0}.json'.format(template.name)),
            json.dumps({
                'files': {
                    n: {
                        m.identifier: out_filename(template, n, m)
                        for m in modes
                    } for n in n_range
                },
                'name': template.name,
                'x_axis_label': template.property('x_axis_label'),
                'desc': template.property('desc'),
                'modes': {m.identifier: m.description() for m in modes}
            })
        )


if __name__ == '__main__':
    main()
