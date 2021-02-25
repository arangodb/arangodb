Adding a new stemming algorithm
===============================

This needs PRs against three repositories.  Name the branch the same for
at least `snowball` and `snowball-data`, push to the latter repo first, and the
CI should use your new vocabulary list when running the testsuite.

Some points to note about algorithm implementations:

* Avoid literal non-ASCII characters in snowball string literals - they will
  work OK for languages that use UTF-8, but not wide-character Unicode or other
  encodings.  Instead use ``stringdef`` like the existing stemmers do, and
  please use the newer `U+` notation rather than the older ``hex`` or
  ``decimal`` as this allows us to support different encodings without having
  to modify the source files - for example::

    stringdef o" {U+00F6}
    define foo 'o{o"}'

  not::

    stringdef o" hex F6
    define foo 'o{o"}'

  and definitely not::

    define foo 'oö'

  It's helpful to consistently use the same ``stringdef`` codes across the
  different stemmers - the website has `guidance on what to use
  <https://snowballstem.org/codesets/guide.html>`_ and a `list of stringdef
  lines for common characters to cut and paste from
  <https://snowballstem.org/codesets/latin-stringdef-list.txt>`_.

snowball repo
-------------

Add `.sbl` source to algorithms subdirectory.

Add entry to `libstemmer/modules.txt`, maintaining the current sorted order by
the first column.  The columns are:

* Algorithm name (needs to match the `.sbl` source without extension)
* Encodings to support.  Wide-character Unicode is always supported
  and doesn't need to be listed here.  You should always include `UTF_8`, and
  also `ISO_8859_1` if the stemmer only uses characters from that and the
  language can be usefully written using it.  We currently also have support
  for `ISO_8859_2` and `KOI8_R`, but other single-byte character sets can be
  supported quite easily if they are useful.
* Names and ISO-639 codes for the language.  Wikipedia has a handy list of `all
  the ISO-639 codes <https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes>`_ -
  find the row for your new language and include the codes from the "639-1",
  "639-2/T" and (if different) "639-2/B" columns.  For example, for the `Afar`
  language you'd put `afar,aa,aar` here.

snowball-data repo
------------------

Add subdirectory named after new stemmer containing:

* voc.txt - word list
* output.txt - stemmed equivalents
* COPYING - licensing details (word lists need to be under an OSI-approved
  licence)

If you don't have access to a suitably licensed word list of a suitable size,
you may be able to use the `wikipedia-most-common-words` script to generate
one by extracting the most frequent words from a Wikipedia dump in the
language the stemmer is for.  If the language uses a script/alphabet which
isn't already supported you may need to add a regular new regular expression.

snowball-website repo
---------------------

Create subdirectory of `algorithms/` named after the language.

Create `stemmer.tt` which describes the stemming algorithm.  This is a
"template toolkit" template which is essentially a mix of HTML and some
macros for adding the navigation, sample vocabulary, etc.  See the
existing `stemmer.tt` files for other algorithms for inspiration.

If it is based on an academic paper, cite the paper and describe any difference
between your implementation and that described in the paper (for example,
sometimes papers have ambiguities that need resolving to re-implement the
algorithm described).

If you have a stopword list, add that as `stop.txt` and link to it from
`stemmer.tt`.

Link to your new `stemmer.tt` from `algorithms/index.tt`.

Add a news entry to `index.tt`.

.. FIXME: Also needs adding for the online demo.

Adding a new programming language generator
===========================================

This is a short guide to adding support for generating code for another
programming language.

Is a new generator the right solution?
--------------------------------------

Adding a new code generator is probably not your only option if you want
to use Snowball from another language - most languages have support for
writing bindings to a C library, so this is probably another option.

Generating code can have advantages.  For example, it can be simpler to
deploy without C bindings which need to be built for a specific platform.

However, it's likely to be significantly more work to implement a new generator
than to write bindings to the generated C code, especially as the libstemmer
C API is a very small and simple one.  Generated code can also be slower -
currently the Snowball compiler often generates code that assumes an optimising
compiler will clean up redundant constructs, which is not a problem for C, and
probably not for most compiled languages, but for a language like Python C
bindings are much faster than the generated Python code (using pypy helps a
lot, but is still slower).  See doc/libstemmer_python_README for some timings.

That said, the unoptimised generated code has improved over time, and is likely
to improve further in the future.

Key problems to solve
---------------------

A key problem to solve is how to map the required flow of control in response
to Snowball signals.

In the generated C code this is mostly done using `goto`.  If your language
doesn't provide an equivalent to `goto` then you'll need an alternative
solution.

In Java and JavaScript we use labelled `break` from blocks and loops
instead.  If your language has an equivalent to this feature, that will
probably work.

For Python, we currently generate a `try:` ... `raise lab123` ...
`except lab123: pass` construct.  This works, but doesn't seem ideal.

If one of the mechanisms above sounds suitable then take a look at the
generator for the respective generated output and generator code.  If
not, come and talk to us on the snowball-discuss mailing list.

Mechanics of adding a new generator
-----------------------------------

Copy an existing `compiler/generator_*.c` for your new language and modify
away (`generator.c` has the generator for C, but also some common functions
so if you start from this one you'll need to remove those common functions).
Please resist reformatting existing code - there's currently a lot of code
repeated in each generator which ought to be pulled out as common code, and
if you reformat that just makes that job harder.

Add your new source to `COMPILER_SOURCES` in `GNUmakefile`.

Add prototypes for the new functions to `compiler/header.h`.

Add support to `compiler/driver.c`.

Add targets to `GNUmakefile` to run tests for the new language.

Hook up automated testing via CI in `.travis.yml`.
