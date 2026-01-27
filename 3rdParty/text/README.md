# text

### Motivation
C and C++ are the only major production languages that do not have robust Unicode support. This library attempts to provide that support for C++.

This library provides a Unicode implementation suitable for use by anyone — including those who know next to nothing about Unicode.

### Features
- Iterators, views, and algorithms that convert among UTF-8, UTF-16, and UTF-32.

- An implementation of the Unicode algorithms for dealing with encodings, normalization, text segmentation, etc.

- A family of types for use in Unicode-aware text processing. This includes text, text_view, rope, and rope_view.

- Code familiarity -- all of the above types and algorithms work like the STL containers and algorithms.

This library targets submission to Boost and eventual standardization.

It's more interesting than it sounds.

Online docs: https://tzlaine.github.io/text

[![Build Status](https://travis-ci.org/tzlaine/text.svg?branch=master)](https://travis-ci.org/tzlaine/text)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/tzlaine/text?branch=master&svg=true)](https://ci.appveyor.com/project/tzlaine/text)
[![codecov](./coverage.svg)](https://codecov.io/gh/tzlaine/text)
[![License](https://img.shields.io/badge/license-boost-brightgreen.svg)](LICENSE_1_0.txt)
