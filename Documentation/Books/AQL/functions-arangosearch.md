---
layout: default
description: ArangoSearch is integrated into AQL and used mainly through the use of special functions.
title: ArangoSearch related AQL Functions
redirect_from:
  - /3.5/views-arango-search-scorers.html # 3.4 -> 3.5
  - /3.5/aql/views-arango-search.html # 3.4 -> 3.5
---
ArangoSearch Functions
======================

Most ArangoSearch AQL functions take an expression or attribute path expression
as argument.

If an expression is expected, it means that search conditions can expressed in
AQL syntax. They are typically function calls to ArangoSearch search functions,
possibly nested and/or using logical operators for multiple conditions.

You need the `ANALYZER()` function to wrap search (sub-)expressions to set the
Analyzer for it, unless you want to use the default `"identity"` Analyzer.
You might not need other ArangoSearch functions for certain expressions,
because comparisons can be done with basic AQL comparison operators.

If an attribute path expressions is needed, then you have to reference a
document object emitted by a View like `FOR doc IN viewName` and the specify
which attribute you want to test for. For example `doc.attr` or
`doc.deeply.nested.attr`. You can also use the bracket notation `doc["attr"]`.

Search Functions
----------------

Search functions can be used in a [SEARCH operation](operations-search.html)
to form an ArangoSearch expression to filter a View. The functions control the
ArangoSearch functionality without having a returnable value in AQL.

The `TOKENS()` function is an exception. It can be used standalone as well,
without a `SEARCH` statement, and has a return value which can be used
elsewhere in the query.

### ANALYZER()

`ANALYZER(expr, analyzer)`

Sets the Analyzer for the given search expression. The default Analyzer is
`identity` for any ArangoSearch expression. This utility function can be used
to wrap a complex expression to set a particular Analyzer. It also sets it for
all the nested functions which require such an argument to avoid repeating the
Analyzer parameter. If an Analyzer argument is passed to a nested function
regardless, then it takes precedence over the Analyzer set via `ANALYZER()`.

The `TOKENS()` function is an exception, it requires the Analyzer name to be
passed in all cases even if wrapped in an `ANALYZER()` call.

- **expr** (expression): any valid search expression
- **analyzer** (string): name of an [Analyzer](../arangosearch-analyzers.html).
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

Assuming a View definition with an Analyzer whose name and type is `delimiter`:

```json
{
  "links": {
    "coll": {
      "analyzers": [ "delimiter" ],
      "includeAllFields": true,
    }
  },
  ...
}
```

… with the Analyzer properties `{ "delimiter": "|" }` and an example document
`{ "text": "foo|bar|baz" }` in the collection `coll`, the following query would
return the document:

```js
FOR doc IN viewName
  SEARCH ANALYZER(doc.text == "bar", "delimiter")
  RETURN doc
```

The expression `doc.text == "bar"` has to be wrapped by `ANALYZER()` in order
to set the Analyzer to `delimiter`. Otherwise the expression would be evaluated
with the default `identity` Analyzer. `"foo|bar|baz" == "bar"` would not match,
but the View does not even process the indexed fields with the `identity`
Analyzer. The following query would also return an empty result because of
the Analyzer mismatch:

```js
FOR doc IN viewName
  SEARCH doc.text == "foo|bar|baz"
  //SEARCH ANALYZER(doc.text == "foo|bar|baz", "identity")
  RETURN doc
```

In below query, the search expression is swapped by `ANALYZER()` to set the
`text_en` Analyzer for both `PHRASE()` functions:

```js
FOR doc IN viewName
  SEARCH ANALYZER(PHRASE(doc.text, "foo") OR PHRASE(doc.text, "bar"), "text_en")
  RETURN doc
```

Without the usage of `ANALYZER()`:

```js
FOR doc IN viewName
  SEARCH PHRASE(doc.text, "foo", "text_en") OR PHRASE(doc.text, "bar", "text_en")
  RETURN doc
```

In the following example `ANALYZER()` is used to set the Analyzer `text_en`,
but in the second call to `PHRASE()` a different Analyzer is set (`identity`)
which overrules `ANALYZER()`. Therefore, the `text_en` Analyzer is used to find
the phrase *foo* and the `identity` Analyzer to find *bar*:

```js
FOR doc IN viewName
  SEARCH ANALYZER(PHRASE(doc.text, "foo") OR PHRASE(doc.text, "bar", "identity"), "text_en")
  RETURN doc
```

Despite the wrapping `ANALYZER()` function, the Analyzer name can not be
omitted in calls to the `TOKENS()` function. Both occurrences of `text_en`
are required, to set the Analyzer for the expression `doc.text IN ...` and
for the `TOKENS()` function itself:

```js
FOR doc IN viewName
  SEARCH ANALYZER(doc.text IN TOKENS("foo", "text_en"), "text_en")
  RETURN doc
```

### BOOST()

`BOOST(expr, boost)`

Override boost in the context of a search expression with a specified value,
making it available for scorer functions. By default, the context has a boost
value equal to `1.0`.

- **expr** (expression): any valid search expression
- **boost** (number): numeric boost value
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

```js
FOR doc IN viewName
  SEARCH ANALYZER(BOOST(doc.text == "foo", 2.5) OR doc.text == "bar", "text_en")
  LET score = BM25(doc)
  SORT score DESC
  RETURN { text: doc.text, score }
```

Assuming a View with the following documents indexed and processed by the
`text_en` Analyzer:

```js
{ "text": "foo bar" }
{ "text": "foo" }
{ "text": "bar" }
{ "text": "foo baz" }
{ "text": "baz" }
```

… the result of above query would be:

```json
[
  {
    "text": "foo bar",
    "score": 2.787301540374756
  },
  {
    "text": "foo baz",
    "score": 1.6895781755447388
  },
  {
    "text": "foo",
    "score": 1.525835633277893
  },
  {
    "text": "bar",
    "score": 0.9913395643234253
  }
]
```

### EXISTS()

{% hint 'info' %}
`EXISTS()` will only match values when the specified attribute has been
processed with the link property **storeValues** set to `"id"` in the
View definition (the default is `"none"`).
{% endhint %}

`EXISTS(path)`

Match documents where the attribute at **path** is present.

- **path** (attribute path expression): the attribute to test in the document
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

```js
FOR doc IN viewName
  SEARCH EXISTS(doc.text)
  RETURN doc
```

`EXISTS(path, type)`

Match documents where the attribute at **path** is present _and_ is of the
specified data type.

- **path** (attribute path expression): the attribute to test in the document
- **type** (string): data type to test for, can be one of:
    - `"null"`
    - `"bool"` / `"boolean"`
    - `"numeric"`
    - `"string"`
    - `"analyzer"` (see below)
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

```js
FOR doc IN viewName
  SEARCH EXISTS(doc.text, "string")
  RETURN doc
```

`EXISTS(path, "analyzer", analyzer)`

Match documents where the attribute at **path** is present _and_ was indexed
by the specified **analyzer**.

- **path** (attribute path expression): the attribute to test in the document
- **type** (string): string literal `"analyzer"`
- **analyzer** (string, _optional_): name of an [Analyzer](../arangosearch-analyzers.html).
  Uses the Analyzer of a wrapping `ANALYZER()` call if not specified or
  defaults to `"identity"`
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

```js
FOR doc IN viewName
  SEARCH EXISTS(doc.text, "analyzer", "text_en")
  RETURN doc
```

### IN_RANGE()

`IN_RANGE(path, low, high, includeLow, includeHigh)`

Match documents where the attribute at **path** is greater than (or equal to)
**low** and less than (or equal to) **high**.

*low* and *high* can be numbers or strings (technically also `null`, `true`
and `false`), but the data type must be the same for both.

- **path** (attribute path expression):
  the path of the attribute to test in the document
- **low** (number\|string): minimum value of the desired range
- **high** (number\|string): maximum value of the desired range
- **includeLow** (bool): whether the minimum value shall be included in
  the range (left-closed interval) or not (left-open interval)
- **includeHigh** (bool): whether the maximum value shall be included in
  the range (right-closed interval) or not (right-open interval)
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

If *low* and *high* are the same, but *includeLow* and/or *includeHigh* is set
to true, then nothing will match. If *low* is greater than *high* nothing will
match either.

To match documents with the attribute `value >= 3` and `value <= 5` using the
default `"identity"` Analyzer you would write the following query:

```js
FOR doc IN viewName
  SEARCH IN_RANGE(doc.value, 3, 5, true, true)
  RETURN doc.value
```

This will also match documents which have an array of numbers as `value`
attribute where at least one of the numbers is in the specified boundaries.

Using string boundaries and a text Analyzer allows to match documents which
have at least one token within the specified character range:

```js
FOR doc IN valView
  SEARCH ANALYZER(IN_RANGE(doc.value, "a","f", true, false), "text_en")
  RETURN doc
```

This will match `{ "value": "bar" }` and `{ "value": "foo bar" }` because the
_b_ of _bar_ is in the range (`"a" <= "b" < "f"`), but not `{ "value": "foo" }`
because the _f_ of _foo_ is excluded (*high* is "f" but *includeHigh* is false).

### MIN_MATCH()

`MIN_MATCH(expr1, ... exprN, minMatchCount)`

Match documents where at least **minMatchCount** of the specified
search expressions are satisfied.

- **expr** (expression, _repeatable_): any valid search expression
- **minMatchCount** (number): minimum number of search expressions that should
  be satisfied
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

Assuming a View with a text Analyzer, you may use it to match documents where
the attribute contains at least two out of three tokens:

```js
FOR doc IN viewName
  SEARCH ANALYZER(MIN_MATCH(doc.text == 'quick', doc.text == 'brown', doc.text == 'fox', 2), "text_en")
  RETURN doc.text
```

This will match `{ "text": "the quick brown fox" }` and `{ "text": "some brown fox" }`,
but not `{ "text": "snow fox" }` which only fulfills one of the conditions.

### PHRASE()

`PHRASE(path, phrasePart, analyzer)`

`PHRASE(path, phrasePart1, skipTokens1, ... phrasePartN, skipTokensN, analyzer)`

Search for a phrase in the referenced attribute. It only matches documents in
which the tokens appear in the specified order. To search for tokens in any
order use [TOKENS()](#tokens) instead.

The phrase can be expressed as an arbitrary number of *phraseParts* separated by
*skipTokens* number of tokens (wildcards).

- **path** (attribute path expression): the attribute to test in the document
- **phrasePart** (string): text to search for in the tokens. May consist of
  several words/tokens, which will be split using the specified *analyzer*
- **skipTokens** (number, _optional_): amount of words/tokens to treat
  as wildcards
- **analyzer** (string, _optional_): name of an [Analyzer](../arangosearch-analyzers.html).
  Uses the Analyzer of a wrapping `ANALYZER()` call if not specified or
  defaults to `"identity"`
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

Given a View indexing an attribute *text* with the `"text_en"` Analyzer and a
document `{ "text": "Lorem ipsum dolor sit amet, consectetur adipiscing elit" }`,
the following query would match it:

```js
FOR doc IN viewName
  SEARCH PHRASE(doc.text, "lorem ipsum", "text_en")
  RETURN doc.text
```

However, this search expression does not because the tokens `"ipsum"` and
`"lorem"` do not appear in this order:

```js
PHRASE(doc.text, "ipsum lorem", "text_en")
```

To match `"ipsum"` and `"amet"` with any two tokens in between, you can use the
following search expression:

```js
PHRASE(doc.text, "ipsum", 2, "amet", "text_en")
```

The *skipTokens* value of `2` defines how many wildcard tokens have to appear
between *ipsum* and *amet*. A *skipTokens* value of `0` means that the tokens
must be adjacent. Negative values are allowed, but not very useful. These three
search expressions are equivalent:

```js
PHRASE(doc.text, "lorem ipsum", "text_en")
PHRASE(doc.text, "lorem", 0, "ipsum", "text_en")
PHRASE(doc.text, "ipsum", -1, "lorem", "text_en")
```

### STARTS_WITH()

`STARTS_WITH(path, prefix)`

Match the value of the attribute that starts with **prefix**. If the attribute
is processed by a tokenizing Analyzer (type `"text"` or `"delimiter"`) or if it
is an array, then a single token/element starting with the prefix is sufficient
to match the document.

- **path** (attribute path expression): the path of the attribute to compare
  against in the document
- **prefix** (string): a string to search at the start of the text
- returns nothing: the function can only be called in a
  [SEARCH operation](operations-search.html) and throws an error otherwise

To match a document `{ "text": "lorem ipsum..." }` using a prefix and the
`"identity"` Analyzer you can use it like this:

```js
FOR doc IN viewName
  SEARCH STARTS_WITH(doc.text, "lorem ip")
  RETURN doc
```

This query will match `{ "text": "lorem ipsum" }` as well as
`{ "text": [ "lorem", "ipsum" ] }` given a View which indexes the `text`
attribute and processes it with the `"text_en"` Analyzer:

```js
FOR doc IN viewName
  SEARCH ANALYZER(STARTS_WITH(doc.text, "ips"), "text_en")
  RETURN doc.text
```

Note that it will not match `{ "text": "IPS (in-plane switching)" }` because
the Analyzer has stemming enabled, but the prefix was passed in as-is:

```js
RETURN TOKENS("IPS (in-plane switching)", "text_en")
```

```json
[
  [
    "ip",
    "in",
    "plane",
    "switch"
  ]
]
```

The *s* is removed from *ips*, which leads to the prefix *ips* not matching
the indexed token *ip*. You may either create a custom text Analyzer with
stemming disabled to avoid this issue, or apply stemming to the prefix:

```js
FOR doc IN viewName
  SEARCH ANALYZER(STARTS_WITH(doc.text, TOKENS("ips", "text_en")[0]), "text_en")
  RETURN doc.text
```

### TOKENS()

`TOKENS(input, analyzer) → tokenArray`

Split the **input** string with the help of the specified **analyzer** into an
array. The resulting array can be used in `FILTER` or `SEARCH` statements with
the `IN` operator, but also be assigned to variables and returned. This can be
used to better understand how a specific Analyzer processes an input value.

It has a regular return value unlike all other ArangoSearch AQL functions and
is thus not limited to `SEARCH` operations. It is independent of Views.
A wrapping `ANALYZER()` call in a search expression does not affect the
*analyzer* argument nor allow you to omit it.

- **input** (string): text to tokenize
- **analyzer** (string): name of an [Analyzer](../arangosearch-analyzers.html).
- returns **tokenArray** (array): array of strings with zero or more elements,
  each element being a token.

Example query showcasing the `"text_de"` Analyzer (tokenization with stemming,
case conversion and accent removal for German text):

```js
RETURN TOKENS("Lörem ipsüm, DOLOR SIT Ämet.", "text_de")
```

```json
[
  [
    "lor",
    "ipsum",
    "dolor",
    "sit",
    "amet"
  ]
]
```

To search a View for documents where the `text` attribute contains certain
words/tokens in any order, you can use the function like this:

```js
FOR doc IN viewName
  SEARCH ANALYZER(doc.text IN TOKENS("dolor amet lorem", "text_en"), "text_en")
  RETURN doc
```

It will match `{ "text": "Lorem ipsum, dolor sit amet." }` for instance. If you
want to search for tokens in a particular order, use [PHRASE()](#phrase) instead.

Scoring Functions
-----------------

Scoring functions return a ranking value for the documents found by a
[SEARCH operation](operations-search.html). The better the documents match
the search expression the higher the returned number.

The first argument to any scoring function is always the document emitted by
a `FOR` operation over an ArangoSearch View.

To sort the result set by relevance, with the more relevant documents coming
first, sort in **descending order** by the score (e.g. `SORT BM25(...) DESC`).

You may calculate custom scores based on a scoring function using document
attributes and numeric functions (e.g. `TFIDF(doc) * LOG(doc.value)`):

```js
FOR movie IN imdbView
  SEARCH PHRASE(movie.title, "Star Wars", "text_en")
  SORT BM25(movie) * LOG(movie.runtime + 1) DESC
  RETURN movie
```

Sorting by more than one score is allowed. You may also sort by a mix of
scores and attributes from multiple Views as well as collections:

```js
FOR a IN viewA
  FOR c IN coll
    FOR b IN viewB
      SORT TFIDF(b), c.name, BM25(a)
      ...
```

### BM25()

`BM25(doc, k, b) → score`

Sorts documents using the
[**Best Matching 25** algorithm](https://en.wikipedia.org/wiki/Okapi_BM25){:target="_blank"}
(BM25).

- **doc** (document): must be emitted by `FOR ... IN viewName`
- **k** (number, _optional_): calibrates the text term frequency scaling.
  The default is `1.2`. A *k* value of `0` corresponds to a binary model
  (no term frequency), and a large value corresponds to using raw term frequency
- **b** (number, _optional_): determines the scaling by the total text length.
  The default is `0.75`. At the extreme values of the coefficient *b*, BM25
  turns into the ranking functions known as:
  - BM11 for *b* = `1` (corresponds to fully scaling the term weight by the
    total text length)
  - BM15 for *b* = `0` (corresponds to no length normalization)
- returns **score** (number): computed ranking value

Sorting by relevance with BM25 at default settings:

```js
FOR doc IN viewName
  SEARCH ...
  SORT BM25(doc) DESC
  RETURN doc
```

Sorting by relevance, with double-weighted term frequency and with full text
length normalization:

```js
FOR doc IN viewName
  SEARCH ...
  SORT BM25(doc, 2.4, 1) DESC
  RETURN doc
```

### TFIDF()

`TFIDF(doc, normalize) → score`

Sorts documents using the
[**term frequency–inverse document frequency** algorithm](https://en.wikipedia.org/wiki/TF-IDF){:target="_blank"}
(TF-IDF).

- **doc** (document): must be emitted by `FOR ... IN viewName`
- **normalize** (bool, _optional_): specifies whether scores should be
  normalized. The default is *false*.
- returns **score** (number): computed ranking value

Sort by relevance using the TF-IDF score:

```js
FOR doc IN viewName
  SEARCH ...
  SORT TFIDF(doc) DESC
  RETURN doc
```

Sort by relevance using a normalized TF-IDF score:

```js
FOR doc IN viewName
  SEARCH ...
  SORT TFIDF(doc, true) DESC
  RETURN doc
```

Sort by the value of the `text` attribute in ascending order, then by the TFIDF
score in descending order where the attribute values are equivalent:

```js
FOR doc IN viewName
  SEARCH ...
  SORT doc.text, TFIDF(doc) DESC
  RETURN doc
```
