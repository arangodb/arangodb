ArangoSearch Scorers
====================

ArangoSearch Scorers are special functions that allow to sort documents from a
view by their score regarding the analyzed fields.

Details about their usage in AQL can be found in the
[ArangoSearch `SORT` section](../../../AQL/Views/ArangoSearch/index.html#arangosearch-sorting).

- BM25: order results based on the [BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

- TFIDF: order results based on the [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)

### `BM25()` - Best Matching 25 Algorithm

IResearch provides a 'bm25' scorer implementing the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25). Optionally, free
parameters **k** and **b** of the algorithm typically using for advanced
optimization can be specified as floating point numbers.

`BM25(doc, k, b)`

- *doc* (document): must be emitted by `FOR doc IN someView`

- *k* (number, _optional_): term frequency, the default is _1.2_. *k*
  calibrates the text term frequency scaling. A *k* value of *0* corresponds to
  a binary model (no term frequency), and a large value corresponds to using raw
  term frequency.

- *b* (number, _optional_): determines the scaling by the total text length, the
  default is _0.75_. *b* determines the scaling by the total text length.
  - b = 1 corresponds to fully scaling the term weight by the total text length
  - b = 0 corresponds to no length normalization.

At the extreme values of the coefficient *b*, BM25 turns into the ranking
functions known as BM11 (for b = 1) and BM15 (for b = 0).

### `TFIDF()` - Term Frequency – Inverse Document Frequency Algorithm

Sorts documents using the
[**term frequency–inverse document frequency** algorithm](https://en.wikipedia.org/wiki/TF-IDF).

`TFIDF(doc, withNorms)`

- *doc* (document): must be emitted by `FOR doc IN someView`
- *withNorms* (bool, _optional_): specifying whether norms should be used via
  **with-norms**, the default is _false_
