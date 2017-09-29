#!/usr/bin/env python

from distutils.core import setup

setup(name='snowballstemmer',
      version='1.2.0',
      description='This package provides 18 stemmer algorithms (17 + Porter English stemmer) generated from Snowball algorithms.',
      long_description='''
It includes following language algorithms:

* Arabic
* Danish
* Dutch
* English (Standard, Porter)
* Finnish
* French
* German
* Hungarian
* Irish
* Italian
* Norwegian
* Portuguese
* Romanian
* Russian
* Spanish
* Swedish
* Turkish

This is a pure Python stemming library. If `PyStemmer <http://pypi.python.org/pypi/PyStemmer>`_ is available, this module uses
it to accelerate.
''',
      author='Yoshiki Shibukawa',
      author_email='yoshiki at shibu.jp',
      url='https://github.com/shibukawa/snowball_py',
      keywords="stemmer",
      license="BSD",
      packages=['snowballstemmer'],
      package_dir={"snowballstemmer": "src/snowballstemmer"},
      classifiers = [
          'Development Status :: 5 - Production/Stable',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: BSD License',
          'Programming Language :: Python',
          'Natural Language :: Arabic',
          'Natural Language :: Danish',
          'Natural Language :: Dutch',
          'Natural Language :: English',
          'Natural Language :: Finnish',
          'Natural Language :: French',
          'Natural Language :: German',
          'Natural Language :: Hungarian',
          'Natural Language :: Italian',
          'Natural Language :: Norwegian',
          'Natural Language :: Portuguese',
          'Natural Language :: Romanian',
          'Natural Language :: Russian',
          'Natural Language :: Spanish',
          'Natural Language :: Swedish',
          'Natural Language :: Turkish',
          'Operating System :: OS Independent',
          'Programming Language :: Python :: 2',
          'Programming Language :: Python :: 3',
          'Topic :: Database',
          'Topic :: Internet :: WWW/HTTP :: Indexing/Search',
          'Topic :: Text Processing :: Indexing',
          'Topic :: Text Processing :: Linguistic'
     ]
)
