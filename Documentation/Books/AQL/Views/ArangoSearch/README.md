ArangoSearch Views in AQL
=========================

Views of type **arangosearch** are an integration layer meant to seamlessly
integrate with and natively expose the full power of the
[IResearch library](https://github.com/iresearch-toolkit/iresearch)
to the ArangoDB user.

They provide the capability to:
* evaluate together documents located in different collections
* filter documents based on AQL boolean expressions and functions
* sort the result set based on how closely each document matched the filter

ArangoSearch value analysis
---------------------------

A concept of value 'analysis' that is meant to break up a given value into
a set of sub-values internally tied together by metadata which influences both
the filter and sort stages to provide the most appropriate match for the
specified conditions, similar to queries to web search engines.

In plain terms this means a user can for example:
* request documents where the 'body' attribute best matches 'a quick brown fox'
* request documents where the 'dna' attribute best matches a DNA sub sequence
* request documents where the 'name' attribute best matches gender
* etc... (via custom analyzers described in the next section)

To a limited degree the concept of 'analysis' is even available in
non-ArangoSearch AQL, e.g. the TOKENS(...) function will utilize the power of
IResearch to break up a value into an AQL array that can be used anywhere in the
AQL query.

In plain terms this means a user can match a document attribute when its
value matches at least one entry from a set,
e.g. to match docs with 'word == quick' OR 'word == brown' OR 'word == fox'

    FOR doc IN someCollection
      FILTER doc.word IN TOKENS('a quick brown fox', 'text_en')
      RETRUN doc

ArangoSearch filters
--------------------

The basic ArangoSearch functionality can be accessed via common AQL filters and
operators, e.g.:

- *AND*
- *OR*
- *NOT*
- *==*
- *<=*
- *>=*
- *<*
- *>*
- *!=*
- *IN <ARRAY>*
- *IN <RANGE>*

However, the full power of ArangoSearch is harnessed and exposed via functions,
during both the filter and sort stages.

The supported AQL context functions are:

### ANALYZER()

`ANALYZER(filter-expression, analyzer)`

Override analyzer in a context of **filter-expression** with another one, denoted
by a specified **analyzer** argument, making it available for filter functions.

- *filter-expression* - any valid filter expression
- *analyzer* - string with the analyzer to imbue, i.e. *"text_en"* or one of the other
  [available string analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)

By default, context contains `Identity` analyzer.

### BOOST()

`BOOST(filter-expression, boost)`

Override boost in a context of **filter-expression** with a specified value,
making it available for scorer funtions.

- *filter-expression* - any valid filter expression
- *boost* - numeric boost value

By default, context contains boost value equal to `1.0`.

The supported filter functions are:

### EXISTS()

`EXISTS(attribute-name)`

Match documents where the attribute **attribute-name** exists in the document.

`EXISTS(attribute-name, "analyzer" [, analyzer])`

Match documents where the **attribute-name** exists in the document and
was indexed by the specified **analyzer**.
In case if **analyzer** isn't specified, current context analyzer (e.g. specified by
`ANALYZER` function) will be used.

`EXISTS(attribute-name, type)`

Match documents where the **attribute-name** exists in the document
 and is of the specified type.

- *attribute-name* - the path of the attribute to exist in the document
- *analyzer* - string with the analyzer used, i.e. *"text_en"* or one of the other
  [available string analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)
- *type* - data type as string; one of:
    - **bool**
    - **boolean**
    - **numeric**
    - **null**
    - **string**

In case if **analyzer** isn't specified, current context analyzer (e.g. specified by
`ANALYZER` function) will be used.

### PHRASE()

```
PHRASE(attribute-name, 
       phrasePart [, skipTokens, phrasePart [, ... skipTokens, phrasePart]]
       [, analyzer])
```

Search for a phrase in the referenced attributes. 

The phrase can be expressed as an arbitrary number of *phraseParts* separated by *skipToken* number of tokens.

- *attribute-name* - the path of the attribute to compare against in the document
- *phrasePart* - a string to search in the token stream; may consist of several words; will be split using the specified *analyzer*
- *skipTokens* number of words or tokens to treat as wildcards
- *analyzer* - string with the analyzer used, i.e. *"text_en"* or one of the other
  [available string analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)

### STARTS_WITH()

`STARTS_WITH(attribute-name, prefix)`

Match the value of the **attribute-name** that starts with **prefix**

- *attribute-name* - the path of the attribute to compare against in the document
- *prefix* - a string to search at the start of the text

### TOKENS()

`TOKENS(input, analyzer)`

Split the **input** string with the help of the specified **analyzer** into an Array.
The resulting Array can i.e. be used in subsequent `FILTER` statements with the **IN** operator.
This can be used to better understand how the specific analyzer is going to behave.

- *input* string to tokenize
- *analyzer* one of the [available string analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)

### MIN_MATCH()

`MIN_MATCH(filter-expression, [..., filter-expression], min-match-count)`

Match documents where at least **min-match-count** of the specified **filter-expression**s
are satisfied.

- *filter-expression* - any valid filter expression
- *min-match-count* - minimum number of filter-expression that should be satisfied

#### Filtering examples

to match documents which have a 'name' attribute

    FOR doc IN VIEW someView
      FILTER EXISTS(doc.name)
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER EXISTS(doc['name'])
      RETURN doc

to match documents where 'body' was analyzed via the 'text_en' analyzer

    FOR doc IN VIEW someView
      FILTER EXISTS(doc.body, 'analyzer', 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER EXISTS(doc['body'], 'analyzer', 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER ANALYZER(EXISTS(doc['body'], 'analyzer'), 'text_en')
      RETURN doc

to match documents which have an 'age' attribute of type number

    FOR doc IN VIEW someView
      FILTER EXISTS(doc.age, 'numeric')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER EXISTS(doc['age'], 'numeric')
      RETURN doc

to match documents where 'description' contains word 'quick' or word
'brown' and has been analyzed with 'text_en' analyzer

    FOR doc IN VIEW someView
      FILTER ANALYZER(doc.description == 'quick' OR doc.description == 'brown', 'text_en')
      RETURN doc

to match documents where 'description' contains at least 2 of 3 words 'quick', 
'brown', 'fox' and has been analyzed with 'text_en' analyzer

    FOR doc IN VIEW someView
      FILTER ANALYZER(
        MIN_MATCH(doc.description == 'quick', doc.description == 'brown', doc.description == 'fox', 2),
        'text_en'
      )
      RETURN doc

to match documents where 'description' contains a phrase 'quick brown'

    FOR doc IN VIEW someView
      FILTER PHRASE(doc.description, [ 'quick brown' ], 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER PHRASE(doc['description'], [ 'quick brown' ], 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER ANALYZER(PHRASE(doc['description'], [ 'quick brown' ]), 'text_en')
      RETURN doc

to match documents where 'body' contains the phrase consisting of a sequence
like this:
'quick' * 'fox jumps' (where the asterisk can be any single word)

    FOR doc IN VIEW someView
      FILTER PHRASE(doc.body, [ 'quick', 1, 'fox jumps' ], 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER PHRASE(doc['body'], [ 'quick', 1, 'fox jumps' ], 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER ANALYZER(PHRASE(doc['body'], [ 'quick', 1, 'fox jumps' ]), 'text_en')
      RETURN doc

to match documents where 'story' starts with 'In the beginning'

    FOR doc IN VIEW someView
      FILTER STARTS_WITH(doc.story, 'In the beginning')
      RETURN DOC

or

    FOR doc IN VIEW someView
      FILTER STARTS_WITH(doc['story'], 'In the beginning')
      RETURN DOC

to watch the analyzer doing its work

    RETURN TOKENS('a quick brown fox', 'text_en')

to match documents where 'description' best matches 'a quick brown fox'

    FOR doc IN VIEW someView
      FILTER ANALYZER(doc.description IN TOKENS('a quick brown fox', 'text_en'), 'text_en')
      RETURN doc

ArangoSearch sort
-----------------

A major feature of ArangoSearch views is their capability of sorting results
based on the creation-time filter conditions and zero or more sorting functions.
The sorting functions are meant to be user-defined.

Note: Similar to other sorting functions on regular collections the first
  argument to any sorting function is _always_ either the document emmited by 
  the `FOR` statement, or some sub-attribute of it. 

The sorting functions are meant to be user-defined. The following functions are already built in:

### Literal sorting
You can sort documents by simply specifying the *attribute-name* directly, as you do using indices in other places.

### Best Matching 25 Algorithm

`BM25(attribute-name, [k, [b]])`

Sorts documents using the [**Best Matching 25** algorithm](https://en.wikipedia.org/wiki/Okapi_BM25).

Optionally the term frequency **k** and coefficient **b** of the algorithm can be specified as floating point numbers:

- *k* defaults to `1.2`; *k* calibrates the text term frequency scaling.
  A *k* value of *0* corresponds to a binary model (no term frequency),
  and a large value corresponds to using raw term frequency.

- *b* defaults to `0.75`; *b* determines the scaling by the total text length.
  - b = 1 corresponds to fully scaling the term weight by the total text length
  - b = 0 corresponds to no length normalization.
 
At the extreme values of the coefficient *b*, BM25 turns into ranking functions known as BM11 (for b = 1) and BM15 (for b = 0).

### Term Frequency – Inverse Document Frequency Algorithm

`TFIDF(attribute-name, [with-norms])`

Sorts documents using the [**term frequency–inverse document frequency** algorithm](https://en.wikipedia.org/wiki/TF-IDF).

  optionally specifying that norms should be used via **with-norms**

### Sorting examples

to sort documents by the value of the 'name' attribute

    FOR doc IN VIEW someView
      SORT doc.name
      RETURN doc

or

    FOR doc IN VIEW someView
      SORT doc['name']
      RETURN doc

to sort documents via the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

    FOR doc IN VIEW someView
      SORT BM25(doc)
      RETURN doc

to sort documents via the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)
with 'k' = 1.2 and 'b' = 0.75

    FOR doc IN VIEW someView
      SORT BM25(doc, 1.2, 0.75)
      RETURN doc

to sort documents via the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)

    FOR doc IN VIEW someView
      SORT TFIDF(doc)
      RETURN doc

to sort documents via the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) with norms

    FOR doc IN VIEW someView
      SORT TFIDF(doc, true)
      RETURN doc

to sort documents by value of 'name' and then by the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) where 'name' values are
equivalent

    FOR doc IN VIEW someView
      SORT doc.name, TFIDF(doc)
      RETURN doc


Use cases
---------

### Prefix search

The data contained in our view looks like that:

```json
{ "id": 1, "body": "ThisIsAVeryLongWord" }
{ "id": 2, "body": "ThisIsNotSoLong" }
{ "id": 3, "body": "ThisIsShorter" }
{ "id": 4, "body": "ThisIs" }
{ "id": 5, "body": "ButNotThis" }
```

We now want to search for documents where the attribute `body` starts with "ThisIs",

A simple AQL query executing this prefix search:

    FOR doc IN VIEW someView
      FILTER STARTS_WITH(doc.body, 'ThisIs')
      RETURN doc

It will find the documents with the ids `1`, `2`, `3`, `4`, but not `5`.
