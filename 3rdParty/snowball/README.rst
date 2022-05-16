Snowball is a small string processing language for creating stemming algorithms
for use in Information Retrieval, plus a collection of stemming algorithms
implemented using it.

Snowball was originally designed and built by Martin Porter.  Martin retired
from development in 2014 and Snowball is now maintained as a community project.
Martin originally chose the name Snowball as a tribute to SNOBOL, the excellent
string handling language from the 1960s.  It now also serves as a metaphor for
how the project grows by gathering contributions over time.

The Snowball compiler translates a Snowball program into source code in another
language - currently Ada, ISO C, C#, Go, Java, Javascript, Object Pascal,
Python and Rust are supported.

This repository contains the source code for the snowball compiler and the
stemming algorithms.  The snowball compiler is written in ISO C - you'll need
a C compiler which support C99 to build it (but the C code it generates should
work with any ISO C compiler).

See https://snowballstem.org/ for more information about Snowball.

What is Stemming?
=================

Stemming maps different forms of the same word to a common "stem" - for
example, the English stemmer maps *connection*, *connections*, *connective*,
*connected*, and *connecting* to *connect*.  So a searching for *connected*
would also find documents which only have the other forms.

This stem form is often a word itself, but this is not always the case as this
is not a requirement for text search systems, which are the intended field of
use.  We also aim to conflate words with the same meaning, rather than all
words with a common linguistic root (so *awe* and *awful* don't have the same
stem), and over-stemming is more problematic than under-stemming so we tend not
to stem in cases that are hard to resolve.  If you want to always reduce words
to a root form and/or get a root form which is itself a word then Snowball's
stemming algorithms likely aren't the right answer.
