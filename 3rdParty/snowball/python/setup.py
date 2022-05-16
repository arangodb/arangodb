#!/usr/bin/env python

from setuptools import setup
import re

SNOWBALL_VERSION = '2.2.0'

n_stemmers = 0

langs = []
variants = {}
with open('modules.txt') as fp:
    for line in fp.readlines():
        if len(line) <= 1 or line[0] == '#':
            continue
        if line[-1:] == '\n':
            line = line[:-1]
        tokens = re.split(r'\s+', line)
        if len(tokens) < 3:
            print("Bad modules.txt line: " + line)
            continue
        (name, encs, codes) = tokens[:3]
        if len(tokens) > 3:
            variant_of = tokens[3]
            if variant_of in variants:
                variants[variant_of].append(name)
            else:
                variants[variant_of] = [name]
        else:
            langs.append(name)
        n_stemmers += 1

desc = 'This package provides ' + str(n_stemmers) + ' stemmers for ' + \
    str(len(langs)) + ' languages generated from Snowball algorithms.'

classifiers = [
    'Development Status :: 5 - Production/Stable',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: BSD License'
]

for lang in langs:
    lang_titlecase = lang.title()
    # Only classifiers listed in https://pypi.org/classifiers/ are allowed
    if lang_titlecase not in ('Armenian', 'Yiddish'):
        classifiers.append('Natural Language :: ' + lang_titlecase)

classifiers.extend([
    'Operating System :: OS Independent',
    'Programming Language :: Python',
    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 2.6',
    'Programming Language :: Python :: 2.7',
    'Programming Language :: Python :: 3',
    'Programming Language :: Python :: 3.4',
    'Programming Language :: Python :: 3.5',
    'Programming Language :: Python :: 3.6',
    'Programming Language :: Python :: 3.7',
    'Programming Language :: Python :: 3.8',
    'Programming Language :: Python :: 3.9',
    'Programming Language :: Python :: 3.10',
    'Programming Language :: Python :: Implementation :: CPython',
    'Programming Language :: Python :: Implementation :: PyPy',
    'Topic :: Database',
    'Topic :: Internet :: WWW/HTTP :: Indexing/Search',
    'Topic :: Text Processing :: Indexing',
    'Topic :: Text Processing :: Linguistic'
])

setup(name='snowballstemmer',
      version=SNOWBALL_VERSION,
      description=desc,
      author='Snowball Developers',
      author_email='snowball-discuss@lists.tartarus.org',
      url='https://github.com/snowballstem/snowball',
      keywords="stemmer",
      license="BSD-3-Clause",
      packages=['snowballstemmer'],
      package_dir={"snowballstemmer": "src/snowballstemmer"},
      classifiers = classifiers
)
