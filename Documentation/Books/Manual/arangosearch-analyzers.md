---
layout: default
description: Analyzers parse input values and transform them into sets of sub-values, for example by breaking up text into words.
title: ArangoSearch Analyzers
redirect_from:
  - /3.5/views-arango-search-analyzers.html # 3.4 -> 3.5
---
ArangoSearch Analyzers
======================

Analyzers parse input values and transform them into sets of sub-values,
for example by breaking up text into words. If they are used in Views then
the documents' attribute values of the linked collections are used as input
and additional metadata is produced internally. The data can then be used for
searching and sorting to provide the most appropriate match for the specified
conditions, similar to queries to web search engines.

Analyzers can be used on their own to tokenize and normalize strings in AQL
queries with the [`TOKENS()` function](aql/functions-arangosearch.html#tokens).

How analyzers process values depends on their type and configuration.
The configuration is comprised of type-specific properties and list of features.
The features control the additional metadata to be generated to augment View
indexes, to be able to rank results for instance.

Analyzers can be managed via an [HTTP API](http/analyzers.html) and through
a [JavaScript module](appendix-java-script-modules-analyzers.html).

Value Handling
--------------

While most of the Analyzer functionality is geared towards text processing,
there is no restriction to strings as input data type when using them through
Views – your documents could have attributes of any data type after all.

Strings are processed according to the Analyzer, whereas other primitive data
types (`null`, `true`, `false`, numbers) are added to the index unchanged.

The elements of arrays are unpacked, processed and indexed individually,
regardless of the level of nesting. That is, strings are processed by the
configured Analyzer(s) and other primitive values are indexed as-is.

Objects, including any nested objects, are indexed as sub-attributes.
This applies to sub-objects as well as objects in arrays. Only primitive values
are added to the index, arrays and objects can not be searched for.

Also see:
- [SEARCH operation](aql/operations-search.html) on how to query indexed
  values such as numbers and nested values
- [ArangoSearch Views](arangosearch-views.html) for details about how
  compound data types (arrays, objects) get indexed

Analyzer Names
--------------

Each Analyzer has a name for identification with the following
naming conventions, similar to collection names:

- The name must only consist of the letters `a` to `z` (both in lower and
  upper case), the numbers `0` to `9`, underscore (`_`) and dash (`-`) symbols.
  This also means that any non-ASCII names are not allowed.
- It must always start with a letter.
- The maximum allowed length of a name is 64 bytes.
- Analyzer names are case-sensitive.

Custom Analyzers are stored per database, in a system collection `_analyzers`.
The names get prefixed with the database name and two colons, e.g.
`myDB::customAnalyzer`.This does not apply to the globally available
[built-in Analyzers](#built-in-analyzers), which are not stored in an
`_analyzers` collection.

Custom Analyzers stored in the `_system` database can be referenced in queries
against other databases by specifying the prefixed name, e.g.
`_system::customGlobalAnalyzer`. Analyzers stored in databases other than
`_system` can not be accessed from within another database however.

Analyzer Types
--------------

The currently implemented Analyzer types are:

- `identity`: treat value as atom (no transformation)
- `delimiter`: split into tokens at user-defined character
- `stem`: apply stemming to the value as a whole
- `norm`: apply normalization to the value as a whole
- `ngram`: create n-grams from value with user-defined lengths
- `text`: tokenize into words, optionally with stemming,
  normalization and stop-word filtering

Available normalizations are case conversion and accent removal
(conversion of characters with diacritical marks to the base characters).

Feature / Analyzer | Identity | N-gram  | Delimiter | Stem | Norm | Text
-------------------|----------|---------|-----------|------|------|-----
**Tokenization**   | No       | No      | (Yes)     | No   | No   | Yes
**Stemming**       | No       | No      | No        | Yes  | No   | Yes
**Normalization**  | No       | No      | No        | No   | Yes  | Yes

Analyzer Properties
-------------------

The valid attributes/values for the *properties* are dependant on what *type*
is used. For example, the `delimiter` type needs to know the desired delimiting
character(s), whereas the `text` type takes a locale, stop-words and more.

### Identity

An Analyzer applying the `identity` transformation, i.e. returning the input
unmodified.

It does not support any *properties* and will ignore them.

### Delimiter

An Analyzer capable of breaking up delimited text into tokens as per
[RFC 4180](https://tools.ietf.org/html/rfc4180)
(without starting new records on newlines).

The *properties* allowed for this Analyzer are an object with the following
attributes:

- `delimiter` (string): the delimiting character(s)

### Stem

An Analyzer capable of stemming the text, treated as a single token,
for supported languages.

The *properties* allowed for this Analyzer are an object with the following
attributes:

- `locale` (string): a locale in the format
  `language[_COUNTRY][.encoding][@variant]` (square brackets denote optional
  parts), e.g. `"de.utf-8"` or `"en_US.utf-8"`. Only UTF-8 encoding is
  meaningful in ArangoDB.

###  Norm

An Analyzer capable of normalizing the text, treated as a single
token, i.e. case conversion and accent removal.

The *properties* allowed for this Analyzer are an object with the following
attributes:

- `locale` (string): a locale in the format
  `language[_COUNTRY][.encoding][@variant]` (square brackets denote optional
  parts), e.g. `"de.utf-8"` or `"en_US.utf-8"`. Only UTF-8 encoding is
  meaningful in ArangoDB.
- `accent` (boolean, _optional_):
  - `true` to preserve accented characters (default)
  - `false` to convert accented characters to their base characters
- `case` (string, _optional_):
  - `"lower"` to convert to all lower-case characters
  - `"upper"` to convert to all upper-case characters
  - `"none"` to not change character case (default)

### N-gram

An Analyzer capable of producing n-grams from a specified input in a range of
min..max (inclusive). Can optionally preserve the original input.

This Analyzer type can be used to implement substring matching.
Note that it currently supports single-byte characters only.
Multi-byte UTF-8 characters raise an *Invalid UTF-8 sequence* query error.

The *properties* allowed for this Analyzer are an object with the following
attributes:

- `min` (number): unsigned integer for the minimum n-gram length
- `max` (number): unsigned integer for the maximum n-gram length
- `preserveOriginal` (boolean):
  - `true` to include the original value as well
  - `false` to produce the n-grams based on *min* and *max* only

**Example**

With `min` = 4 and `max` = 5, the analyzer will produce the following n-grams
for the input string `"foobar"`:
- `"foobar"` (if `preserveOriginal` is enabled)
- `"fooba"`
- `"foob"`
- `"oobar"`
- `"ooba"`
- `"obar"`

An input string `"foo"` will not produce any n-gram because it is shorter
than the `min` length of 4.

### Text

An Analyzer capable of breaking up strings into individual words while also
optionally filtering out stop-words, extracting word stems, applying
case conversion and accent removal.

Stemming support is provided by
[Snowball](https://snowballstem.org/){:target="_blank"}.

The *properties* allowed for this Analyzer are an object with the following
attributes:

- `locale` (string): a locale in the format
  `language[_COUNTRY][.encoding][@variant]` (square brackets denote optional
  parts), e.g. `"de.utf-8"` or `"en_US.utf-8"`. Only UTF-8 encoding is
  meaningful in ArangoDB.
- `accent` (boolean, _optional_):
  - `true` to preserve accented characters
  - `false` to convert accented characters to their base characters (default)
- `case` (string, _optional_):
  - `"lower"` to convert to all lower-case characters (default)
  - `"upper"` to convert to all upper-case characters
  - `"none"` to not change character case
- `stemming` (boolean, _optional_):
  - `true` to apply stemming on returned words (default)
  - `false` to leave the tokenized words as-is
- `stopwords` (array, _optional_): an array of strings with words to omit
  from result. Default: load words from `stopwordsPath`. To disable stop-word
  filtering provide an empty array `[]`. If both *stopwords* and
  *stopwordsPath* are provided then both word sources are combined.
- `stopwordsPath` (string, _optional_): path with a *language* sub-directory
  (e.g. `en` for a locale `en_US.utf-8`) containing files with words to omit.
  Each word has to be on a separate line. Everything after the first whitespace
  character on a line will be ignored and can be used for comments. The files
  can be named arbitrarily and have any file extension (or none).

  Default: if no path is provided then the value of the environment variable
  `IRESEARCH_TEXT_STOPWORD_PATH` is used to determine the path, or if it is
  undefined then the current working directory is assumed. If the `stopwords`
  attribute is provided then no stop-words are loaded from files, unless an
  explicit *stopwordsPath* is also provided.

  Note that if the *stopwordsPath* can not be accessed, is missing language
  sub-directories or has no files for a language required by an Analyzer,
  then the creation of a new Analyzer is refused. If such an issue is 
  discovered for an existing Analyzer during startup then the server will
  abort with a fatal error.

Analyzer Features
-----------------

The *features* of an Analyzer determine what term matching capabilities will be
available and as such are only applicable in the context of ArangoSearch Views.

The valid values for the features are dependant on both the capabilities of
the underlying *type* and the query filtering and sorting functions that the
result can be used with. For example the *text* type will produce
`frequency` + `norm` + `position` and the `PHRASE()` AQL function requires
`frequency` + `position` to be available.

Currently the following *features* are supported:

- **frequency**: how often a term is seen, required for `PHRASE()`
- **norm**: the field normalization factor
- **position**: sequentially increasing term position, required for `PHRASE()`.
  If present then the *frequency* feature is also required

Built-in Analyzers
------------------

There is a set of built-in analyzers which are available by default for
convenience and backward compatibility. They can not be removed.

The `identity` analyzer has the features `frequency` and `norm`.
The analyzers of type `text` all tokenize strings with stemming enabled,
no stopwords configured, case conversion set to `lower`, accent removal
turned on and the features `frequency`, `norm` and `position`:

Name       | Type       | Language
-----------|------------|-----------
`identity` | `identity` | none
`text_de`  | `text`     | German
`text_en`  | `text`     | English
`text_es`  | `text`     | Spanish
`text_fi`  | `text`     | Finnish
`text_fr`  | `text`     | French
`text_it`  | `text`     | Italian
`text_nl`  | `text`     | Dutch
`text_no`  | `text`     | Norwegian
`text_pt`  | `text`     | Portuguese
`text_ru`  | `text`     | Russian
`text_sv`  | `text`     | Swedish
`text_zh`  | `text`     | Chinese
