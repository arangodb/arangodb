HTTP Interface for Analyzers
============================

The REST API is accessible via the `/_api/analyzer` endpoint URL callable via
HTTP requests.

Analyzer Operations
-------------------

### Create Analyzer

The *create* operation is accessible via the *POST* method on the URL:

    /_api/analyzer

With the Analyzer configuration passed via the body as an object with
attributes:

- *name*: string (required)
- *type*: string (required)
- *properties*: string or object or null (optional) default: `null`
- *features*: array of strings (optional) default: empty array

@startDocuBlock post_api_analyzer

### Get an Analyzer

The *get* operation is accessible via the *GET* method on the URL:

    /_api/analyzer/{analyzer-name}

A successful result will be an object with the fields:
- *name*
- *type*
- *properties*
- *features*

@startDocuBlock get_api_analyzer

### Get all Analyzers

The *list* operation is accessible via the *GET* method on the URL:

    /_api/analyzer

A successful result will be an array of object with the fields:
- *name*
- *type*
- *properties*
- *features*

@startDocuBlock get_api_analyzers

### Remove Analyzer

The *remove* operation is accessible via the *DELETE* method on the URL:

    /_api/analyzer/{analyzer-name}[?force=true]

@startDocuBlock delete_api_analyzer

Analyzer types
--------------

The currently implemented Analyzer types are:

- `identity`
- `delimited`
- `ngram`
- `text`

### Identity

An analyzer applying the `identity` transformation, i.e. returning the input
unmodified.

The value of the *properties* attribute is ignored.

### Delimited

An analyzer capable of breaking up delimited text into tokens as per RFC4180
(without starting new records on newlines).

The *properties* allowed for this analyzer are either:

- a string encoded delimiter to use
- an object with the attribute *delimiter* containing the string encoded
  delimiter to use

### Ngram

An analyzer capable of producing ngrams from a specified input in a range of
[min_gram;max_gram]. Can optionally preserve the original input.

The *properties* allowed for this analyzer are an object with the following
attributes:

- *max*: unsigned integer (required) max ngram
- *min*: unsigned integer (required) min ngram
- *preserveOriginal*: boolean (required) output the original value as well

### Text

An analyzer capable of breaking up strings into individual words while also
optionally filtering out stopwords, applying case conversion and extracting
word stems.

The *properties* allowed for this analyzer are an object with the following
attributes:

- `locale`: string (required) format: (language[_COUNTRY][.encoding][@variant])
- `case_convert`: string enum (optional) one of: `lower`, `none`, `upper`,
  default: `lower`
- `ignored_words`: array of strings (optional) words to omit from result,
  default: load words from `ignored_words_path`
- `ignored_words_path`: string(optional) path with the `language` sub-directory
  containing files with words to omit, default: if no
  `ignored_words` provided then the value from the
  environment variable `IRESEARCH_TEXT_STOPWORD_PATH` or
  if undefined then the current working directory
- `no_accent`: boolean (optional) apply accent removal, default: true
- `no_stem`: boolean (optional) do not apply stemming on returned words,
  default: false
