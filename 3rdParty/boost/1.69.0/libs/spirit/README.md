Spirit
======

Spirit is a set of C++ libraries for parsing and output generation implemented as 
Domain Specific Embedded Languages (DSEL) using Expression templates and Template 
Meta-Programming. The Spirit libraries enable a target grammar to be written 
exclusively in C++. Inline grammar specifications can mix freely with other 
C++ code and, thanks to the generative power of C++ templates, are immediately 
executable.

### Spirit.X3 (3rd generation)

[Documentation](http://www.boost.org/doc/libs/develop/libs/spirit/doc/x3/html/index.html)

The newest Spirit shines faster compile times. Currently only a parser framework.

Requires C++14 compiler (GCC 5, Clang 3.5, VS 2015 Update 3).

### Spirit V2 (2nd generation)

[Documentation](http://www.boost.org/doc/libs/develop/libs/spirit/doc/html/index.html)

The latest Long Term Support version of Spirit. A Swiss Army knife for data
manipulation on any kind of input.

Consists of:
  - [Qi]: Parser framework.
  - [Karma]: Generator framework.
  - [Lex]: Lexical analyzer framework.
  
Runs on most C++03 compilers (GCC 4.1, Clang 3.0, VS 2005).

[Spirit V2]: http://www.boost.org/doc/libs/develop/libs/spirit/doc/html/index.html
[Qi]: http://www.boost.org/doc/libs/develop/libs/spirit/doc/html/spirit/qi.html
[Karma]: http://www.boost.org/doc/libs/develop/libs/spirit/doc/html/spirit/karma.html
[Lex]: http://www.boost.org/doc/libs/develop/libs/spirit/doc/html/spirit/lex.html

### Spirit.Classic (1st generation)

[Documentation](http://www.boost.org/doc/libs/develop/libs/spirit/classic/index.html)

An elderling member of Spirit. It receives only limited maintanance, but
it is still used even inside Boost by [Boost.Serialization] and [Boost.Wave]
libraries. It also contains Phoenix V1.

Spririt.Classic should support even ancient compilers.

[Boost.Serialization]: http://boost.org/libs/serialization
[Boost.Wave]: http://boost.org/libs/wave

## Brief History

Date       | Boost | Commit   | Event
---------- | ----- | -------- | -----------------------------------------------
2014-03-18 | 1.56  | 8a353328 | Spirit.X3 is added
2013-12-14 | 1.56  | c0537c82 | Phoenix V2 is retired
2011-03-28 | 1.47  | 400a764d | [Phoenix V3] support added to Spirit V2
2009-04-30 | 1.41  | 5963a395 | [Spirit.Repository] is appeared
2008-04-13 | 1.36  | ffd0cc10 | Spirit V2 (Qi, Karma, Lex, Phoenix V2) is added
2006-08-23 | 1.35  | 2dc892b4 | Fusion V1 is retired
2003-01-31 | 1.30  | 81907916 | Spirit is the part of the Boost

[Phoenix V3]: http://boost.org/libs/phoenix
[Spirit.Repository]: http://www.boost.org/doc/libs/develop/libs/spirit/doc/html/spirit/repository.html
