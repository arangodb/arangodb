ArangoSearch Views in AQL
=========================

Views of type `arangosearch` are an integration layer meant to seamlessly
integrate with and natively expose the full power of the
[IResearch library](https://github.com/iresearch-toolkit/iresearch)
to the ArangoDB user.

They provide the capability to:

- evaluate together documents located in different collections
- search documents based on AQL boolean expressions and functions
- sort the result set based on how closely each document matched the search

Overview and Significance
-------------------------

Looking up documents in an ArangoSearch View is done via the `FOR` keyword:

```js
FOR doc IN someView
  ...
```

`FOR` operations over ArangoSearch Views have an additional, optional, `SEARCH`
keyword:

```js
FOR doc IN someView
  SEARCH searchExpression
```

ArangoSearch views cannot be used as edge collections in traversals:

```js
FOR v IN 1..3 ANY startVertex someView /* invalid! */
```

### SEARCH

`SEARCH` expressions look a lot like `FILTER` operations, but have some noteable
differences.

First of all, filters and functions in `SEARCH`, when applied to documents
_emitted from an ArangoSearch View_, work _only_ on attributes linked in the
view.

For example, given a collection `myCol` with the following documents:

```js
[
  { someAttr: 'One', anotherAttr: 'One' },
  { someAttr: 'Two', anotherAttr: 'Two' }
]
```

with a view, where `someAttr` is indexed by the following view `myView`:

```js
{
  "type": "arangosearch",
  "links": {
    "myCol": {
      "fields": {
        "someAttr": {}
      }
    }
  }
}
```

Then, a search on `someAttr` yields the following result:

```js
FOR doc IN myView
  SEARCH doc.someAttr == 'One'
  RETURN doc
```

```js
[ { someAttr: 'One', anotherAttr: 'One' } ]
```

While a search on `anotherAttr` yields an empty result:

```js
FOR doc IN myView
  SEARCH doc.anotherAttr == 'One'
  RETURN doc
```

```js
[]
```

- This only applies to the expression after the `SEARCH` keyword.
- This only applies to tests regarding documents emitted from a view. Other
  tests are not affected.
- In order to use `SEARCH` using all attributes of a linked sources, the special
  `includeAllFields` [link property](../../../Manual/Views/ArangoSearch/DetailedOverview.html#link-properties)
  was designed.

### SORT

The document search via the `SEARCH` keyword and the sorting via the
ArangoSearch functions, namely `BM25()` and `TFIDF()`, are closely intertwined.
The query given in the `SEARCH` expression is not only used to filter documents,
but also is used with the sorting functions to decide which document matches
the query best. Other documents in the view also affect this decision.

Therefore the ArangoSearch sorting functions can work _only_ on documents
emitted from a view, as both the corresponding `SEARCH` expression and the view
itself are consulted in order to sort the results.

The `BOOST()` function, described below, can be used to fine-tune the resulting
ranking by weighing sub-expressions in `SEARCH` differently.

### Arrays and trackListPositions

Unless [**trackListPositions**](../../../Manual/Views/ArangoSearch/DetailedOverview.html#link-properties)
is set to `true`, which it is not by default, arrays behave differently. Namely
they behave like a disjunctive superposition of their values - this is best
shown with an example.

With `trackListPositions: false`, which is the default, and given a document
`doc` containing

```js
{ attr: [ 'valueX', 'valueY', 'valueZ' ] }
```

in a `SEARCH` clause, the expression

```js
doc.attr == 'valueX'
```

will be true, as will be

```js
doc.attr == 'valueY'
```

and `== valueZ`. With `trackListPositions: true`,

```js
doc.attr[0] == 'valueX'
```

would work as usual.

### Comparing analyzed fields

As described in [value analysis](#arangosearch-value-analysis), when a field is
processed by a specific analyzer, comparison tests are done per word. For
example, given the field `text` is analyzed with `"text_en"` and contains the
string `"a quick brown fox jumps over the lazy dog"`, the following expression
will be true:

```js
ANALYZER(d.text == 'fox', "text_en")
```

Note also, that the words analyzed in the text are stemmed, so this is also
true:

```js
ANALYZER(d.text == 'jump', "text_en")
```

So a comparison will actually test if a word is contained in the text. With
`trackListPositions: false`, this means for arrays if the word is contained in
any element of the array. For example, given

```js
d.text = [ "a quick", "brown fox", "jumps over the", "lazy dog"]
```

the following will be true:

```js
ANALYZER(d.text == 'jump', "text_en")
```

ArangoSearch value analysis
---------------------------

A concept of value 'analysis' that is meant to break up a given value into
a set of sub-values internally tied together by metadata which influences both
the search and sort stages to provide the most appropriate match for the
specified conditions, similar to queries to web search engines.

In plain terms this means a user can for example:

- request documents where the 'body' attribute best matches 'a quick brown fox'
- request documents where the 'dna' attribute best matches a DNA sub sequence
- request documents where the 'name' attribute best matches gender
- etc. (via custom analyzers)

To a limited degree the concept of 'analysis' is even available in
non-ArangoSearch AQL, e.g. the TOKENS(...) function will utilize the power of
IResearch to break up a value into an AQL array that can be used anywhere in the
AQL query.

In plain terms this means a user can match a document attribute when its
value matches at least one entry from a set,
e.g. to match docs with 'word == quick' OR 'word == brown' OR 'word == fox'

    FOR doc IN someCollection
      FILTER doc.word IN TOKENS('a quick brown fox', 'text_en')
      RETURN doc

ArangoSearch filters
--------------------

The basic ArangoSearch functionality can be accessed via the `SEARCH` statement
with common AQL filters and operators, e.g.:

- `AND`
- `OR`
- `NOT`
- `==`
- `<=`
- `>=`
- `<`
- `>`
- `!=`
- `IN <ARRAY>`
- `IN <RANGE>`

However, the full power of ArangoSearch is harnessed and exposed via functions,
during both the search and sort stages.

Note, that `SEARCH` statement, in contrast to `FILTER`, is meant to be treated
as a part of the `FOR` operation, not as an individual statement.

The supported AQL context functions are:

### ANALYZER()

`ANALYZER(searchExpression, analyzer)`

Override analyzer in a context of **searchExpression** with another one,
denoted by a specified **analyzer** argument, making it available for search
functions.

- *searchExpression* - any valid search expression
- *analyzer* - string with the analyzer to imbue, i.e. *"text_en"* or one of the
  other [available string analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)

By default, context contains `Identity` analyzer.

### BOOST()

`BOOST(searchExpression, boost)`

Override boost in a context of **searchExpression** with a specified value,
making it available for scorer functions.

- *searchExpression* - any valid search expression
- *boost* - numeric boost value

By default, context contains boost value equal to `1.0`.

The supported search functions are:

### EXISTS()

Note: Will only match values when the specified attribute has been processed
with the link property **storeValues** set to **"id"** (by default it's
**"none"**).

`EXISTS(doc.someAttr)`

Match documents **doc** where the attribute **someAttr** exists in the
document.

This also works with sub-attributes, e.g.

`EXISTS(doc.someAttr.anotherAttr)`

as long as the field is processed by the view with **storeValues** not
**none**.

`EXISTS(doc.someAttr, "analyzer", analyzer)`

Match documents where **doc.someAttr** exists in the document _and_ was indexed
by the specified **analyzer**. **analyzer** is optional and defaults to the
current context analyzer (e.g. specified by `ANALYZER` function).

`EXISTS(doc.someAttr, type)`

Match documents where the **doc.someAttr** exists in the document
 and is of the specified type.

- *doc.someAttr* - the path of the attribute to exist in the document
- *analyzer* - string with the analyzer used, i.e. *"text_en"* or one of the
  other [available string analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)
- *type* - data type as string; one of:
    - **bool**
    - **boolean**
    - **numeric**
    - **null**
    - **string**

In case if **analyzer** isn't specified, current context analyzer (e.g.
specified by `ANALYZER` function) will be used.

### PHRASE()

```
PHRASE(doc.someAttr, 
       phrasePart [, skipTokens] [, phrasePart | , phrasePart, skipTokens]*
       [, analyzer])
```

Search for a phrase in the referenced attributes. 

The phrase can be expressed as an arbitrary number of *phraseParts* separated by
*skipToken* number of tokens.

- *doc.someAttr* - the path of the attribute to compare against in the document
- *phrasePart* - a string to search in the token stream; may consist of several
  words; will be split using the specified *analyzer*
- *skipTokens* number of words or tokens to treat as wildcards
- *analyzer* - string with the analyzer used, i.e. *"text_en"* or one of the
  other [available string analyzers
  ](../../../Manual/Views/ArangoSearch/Analyzers.html)

For example, given a document `doc` containing the text `"Lorem ipsum dolor sit
amet, consectetur adipiscing elit"`, the following expression will be `true`:

```js
PHRASE(doc.text, "ipsum", 1, "sit", 2, "adipiscing", "text_de")
```

Specifying deep attributes like `doc.some.deep.attr` is also allowed. The
attribute has to be processed by the view as specified in the link.

### STARTS_WITH()

`STARTS_WITH(doc.someAttr, prefix)`

Match the value of the **doc.someAttr** that starts with **prefix**

- *doc.someAttr* - the path of the attribute to compare against in the document
- *prefix* - a string to search at the start of the text

Specifying deep attributes like `doc.some.deep.attr` is also allowed. The
attribute has to be processed by the view as specified in the link.

### TOKENS()

`TOKENS(input, analyzer)`

Split the **input** string with the help of the specified **analyzer** into an
Array. The resulting Array can i.e. be used in subsequent `FILTER` or `SEARCH`
statements with the **IN** operator. This can be used to better understand how
the specific analyzer is going to behave.
- *input* string to tokenize
- *analyzer* one of the [available string_analyzers](../../../Manual/Views/ArangoSearch/Analyzers.html)

### MIN_MATCH()

`MIN_MATCH(searchExpression [, searchExpression]*, minMatchCount)`

Match documents where at least **minMatchCount** of the specified
**searchExpression**s are satisfied.

- *searchExpression* - any valid search expression
- *minMatchCount* - minimum number of *searchExpression*s that should be
  satisfied

For example,

```js
MIN_MATCH(doc.text == 'quick', doc.text == 'brown', doc.text == 'fox', 2)
```

if `doc.text`, as analyzed by the current analyzer, contains 2 out of 'quick',
'brown' and 'fox', it will be included as matched one.

### Searching examples

to match documents which have a 'name' attribute

    FOR doc IN someView SEARCH EXISTS(doc.name)
      RETURN doc

or

    FOR doc IN someView SEARCH EXISTS(doc['name'])
      RETURN doc

to match documents where 'body' was analyzed via the 'text_en' analyzer

    FOR doc IN someView SEARCH EXISTS(doc.body, 'analyzer', 'text_en')
      RETURN doc

or

    FOR doc IN someView SEARCH EXISTS(doc['body'], 'analyzer', 'text_en')
      RETURN doc

or

    FOR doc IN someView SEARCH ANALYZER(EXISTS(doc['body'], 'analyzer'), 'text_en')
      RETURN doc

to match documents which have an 'age' attribute of type number

    FOR doc IN someView SEARCH EXISTS(doc.age, 'numeric')
      RETURN doc

or

    FOR doc IN someView SEARCH EXISTS(doc['age'], 'numeric')
      RETURN doc

to match documents where 'description' contains word 'quick' or word
'brown' and has been analyzed with 'text_en' analyzer

    FOR doc IN someView SEARCH ANALYZER(doc.description == 'quick' OR doc.description == 'brown', 'text_en')
      RETURN doc

to match documents where 'description' contains at least 2 of 3 words 'quick', 
'brown', 'fox' and has been analyzed with 'text_en' analyzer

    FOR doc IN someView SEARCH ANALYZER(
        MIN_MATCH(doc.description == 'quick', doc.description == 'brown', doc.description == 'fox', 2),
        'text_en'
      )
      RETURN doc

to match documents where 'description' contains a phrase 'quick brown'

    FOR doc IN someView SEARCH PHRASE(doc.description, [ 'quick brown' ], 'text_en')
      RETURN doc

or

    FOR doc IN someView SEARCH PHRASE(doc['description'], [ 'quick brown' ], 'text_en')
      RETURN doc

or

    FOR doc IN someView SEARCH ANALYZER(PHRASE(doc['description'], [ 'quick brown' ]), 'text_en')
      RETURN doc

to match documents where 'body' contains the phrase consisting of a sequence
like this:
'quick' * 'fox jumps' (where the asterisk can be any single word)

    FOR doc IN someView SEARCH PHRASE(doc.body, [ 'quick', 1, 'fox jumps' ], 'text_en')
      RETURN doc

or

    FOR doc IN someView SEARCH PHRASE(doc['body'], [ 'quick', 1, 'fox jumps' ], 'text_en')
      RETURN doc

or

    FOR doc IN someView SEARCH ANALYZER(PHRASE(doc['body'], [ 'quick', 1, 'fox jumps' ]), 'text_en')
      RETURN doc

to match documents where 'story' starts with 'In the beginning'

    FOR doc IN someView SEARCH STARTS_WITH(doc.story, 'In the beginning')
      RETURN DOC

or

    FOR doc IN someView SEARCH STARTS_WITH(doc['story'], 'In the beginning')
      RETURN DOC

to watch the analyzer doing its work

    RETURN TOKENS('a quick brown fox', 'text_en')

to match documents where 'description' best matches 'a quick brown fox'

    FOR doc IN someView SEARCH ANALYZER(doc.description IN TOKENS('a quick brown fox', 'text_en'), 'text_en')
      RETURN doc

ArangoSearch sorting
--------------------

A major feature of ArangoSearch Views is their capability of sorting results
based on the creation-time search conditions and zero or more sorting functions.
The ArangoSearch sorting functions available are `TFIDF()` and `BM25()`.

Note: The first argument to any ArangoSearch sorting function is _always_ the
document emitted by a `FOR` operation over an ArangoSearch View.

Note: An ArangoSearch sorting function is _only_ allowed as an argument to a
`SORT` operation. But they can be mixed with other arguments to `SORT`.

So the following examples are valid:

```js
FOR doc IN someView
    SORT TFIDF(doc)
```

```js
FOR a IN viewA
    FOR b IN viewB
        SORT BM25(a), TFIDF(b)
```

```js
FOR a IN viewA
    FOR c IN someCollection
        FOR b IN viewB
            SORT TFIDF(b), c.name, BM25(a)
```

while these will _not_ work:

```js
FOR doc IN someCollection
    SORT TFIDF(doc) // !!! Error
```
```js
FOR doc IN someCollection
    RETURN BM25(doc) // !!! Error
```
```js
FOR doc IN someCollection
    SORT BM25(doc.someAttr) // !!! Error
```
```js
FOR doc IN someView
    SORT TFIDF("someString") // !!! Error
```
```js
FOR doc IN someView
    SORT BM25({some: obj}) // !!! Error
```

The following sorting methods are available:

### Literal sorting
You can sort documents by simply specifying arbitrary values or expressions, as
you do in other places.

### BM25()

`BM25(doc, k, b)`

- *k* (number, _optional_): calibrates the text term frequency scaling, the default is
_1.2_. A *k* value of _0_ corresponds to a binary model (no term frequency), and a large
value corresponds to using raw term frequency
- *b* (number, _optional_): determines the scaling by the total text length, the default
is _0.75_. At the extreme values of the coefficient *b*, BM25 turns into ranking
functions known as BM11 (for *b* = `1`,  corresponds to fully scaling the term weight by
the total text length) and BM15 (for *b* = `0`, corresponds to no length normalization)

Sorts documents using the [**Best Matching 25** algorithm](https://en.wikipedia.org/wiki/Okapi_BM25).
See the [`BM25()` section in ArangoSearch Scorers](../../../Manual/Views/ArangoSearch/Scorers.html)
for details.

### TFIDF()

`TFIDF(doc, withNorms)`

- *doc* (document): must be emitted by `FOR doc IN someView`
- *withNorms* (bool, _optional_): specifying whether scores should be
  normalized, the default is _false_

Sorts documents using the
[**term frequency–inverse document frequency** algorithm](https://en.wikipedia.org/wiki/TF-IDF).
See the
[`TFIDF()` section in ArangoSearch Scorers](../../../Manual/Views/ArangoSearch/Scorers.html)
for details.


### Sorting examples

to sort documents by the value of the 'name' attribute

    FOR doc IN someView
      SORT doc.name
      RETURN doc

or

    FOR doc IN someView
      SORT doc['name']
      RETURN doc

to sort documents via the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

    FOR doc IN someView
      SORT BM25(doc)
      RETURN doc

to sort documents via the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)
with 'k' = 1.2 and 'b' = 0.75

    FOR doc IN someView
      SORT BM25(doc, 1.2, 0.75)
      RETURN doc

to sort documents via the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)

    FOR doc IN someView
      SORT TFIDF(doc)
      RETURN doc

to sort documents via the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) with norms

    FOR doc IN someView
      SORT TFIDF(doc, true)
      RETURN doc

to sort documents by value of 'name' and then by the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) where 'name' values are
equivalent

    FOR doc IN someView
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

    FOR doc IN someView SEARCH STARTS_WITH(doc.body, 'ThisIs')
      RETURN doc

It will find the documents with the ids `1`, `2`, `3`, `4`, but not `5`.
