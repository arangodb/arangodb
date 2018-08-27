ArangoSearch Scorers
====================

ArangoSearch accesses scorers directly by their internal names. The
name (in upper-case) of the scorer is the function name to be used in the
['SORT' section](../../../AQL/Views/ArangoSearch/index.html#arangosearch-sort).
Function arguments, (excluding the first argument), are serialized as a
string representation of a JSON array and passed directly to the corresponding
scorer. The first argument to any scorer function is the reference to the 
current document emitted by the `FOR` statement, i.e. it would be 'doc' for this
statement:

```js
FOR doc IN someView
```

IResearch provides a 'bm25' scorer implementing the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25). This scorer
optionally takes 'k' and 'b' positional parameters.

The user is able to run queries with the said scorer, e.g.

```js
SORT BM25(doc, 1.2, 0.75)
```

The function arguments will then be serialized into a JSON representation:

```json
[ 1.2, 0.75 ]
```

and passed to the scorer implementation.

Similarly an administrator may choose to deploy a custom DNA analyzer 'DnaRank'.

The user is then immediately able to run queries with the said scorer, e.g.

```js
SORT DNARANK(doc, 123, 456, "abc", { "def": "ghi" })
```

The function arguments will then be serialized into a JSON representation:

```json
[ 123, 456, "abc", { "def": "ghi" } ]
```

and passed to the scorer implementation.

Runtime-plugging functionality for scores is not available in ArangoDB at this
point in time, so ArangoDB comes with a few default-initialized scores:

- *attribute-name*: order results based on the value of **attribute-name**

- BM25: order results based on the [BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

- TFIDF: order results based on the [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)
