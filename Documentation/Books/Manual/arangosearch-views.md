---
layout: default
description: ArangoSearch Views
redirect_from:
  - /3.5/views-arango-search-detailed-overview.html # 3.4 -> 3.5
---
ArangoSearch Views
==================

ArangoSearch Views enable sophisticated information retrieval queries such as
full-text search for unstructured or semi-structured data over documents from
different collections, filtering on multiple document attributes and sorting
the documents that satisfy the search criteria by relevance.

Comparison with the [Full-text Index](indexing-fulltext.html):

Feature                           | ArangoSearch | Full-text Index
----------------------------------|--------------|----------------
Term search                       | Yes          | Yes
Prefix search                     | Yes          | Yes
Boolean expressions               | Yes          | Restricted
Range search                      | Yes          | No
Phrase search                     | Yes          | No
Relevance ranking                 | Yes          | No
Configurable Analyzers            | Yes          | No
AQL composable language construct	| Yes          | No
Indexed attributes per collection | Unlimited    | 1
Indexed collections               | Unlimited    | 1

Views guarantee the best execution plan (merge join) when querying multiple
attributes, unlike collections with user-defined indexes.

Concept
-------

A View can be understood as abstraction over a transformation applied to documents
of zero or more collections. The transformation is View-implementation specific
and may even be as simple as an identity transformation thus making the View
represent all documents available in the specified set of source collections.
Currently there is a single supported View implementation, ArangoSearch Views.

ArangoSearch Views combine two information retrieval models: Boolean and
generalized ranking retrieval. Each document "approved" by Boolean model gets
its own rank from the ranking model.

For text retrieval, the Vector Space Model (VSM) is used as the ranking model.
According to the model, documents and query are represented as vectors in a
space formed by the _terms_ of the query. Typically terms are single words,
keywords or even phrases. Value analysis such as splitting text into words
(tokenization) and normalizing them is possible with the help of
[Analyzers](arangosearch-analyzers.html). As this is application dependent
ArangoDB offers configurable Analyzers aside from a set of built-in Analyzers.

The document vectors that are closer to a query vector are more relevant.
Practically the closeness is expressed as the cosine of the angle between two
vectors, namely [cosine similarity](https://en.wikipedia.org/wiki/Cosine_similarity){:target="_blank"}.

In order to define how relevant document `d` is to the query `q` the following
expression is evaluated:

`cos a = (d * q) / (|d| * |q|)`, where `d * q` is the dot product of the query
vector `q` and document vector `d`, `|d|` is the norm of the vector `d`, `|q|`
is the norm of the vector `q`.

The required vector components have to be computed upfront. Since the space is
formed by the terms, _term weights_ can be used as the coordinates. There are a
number of probability/statistical weighting models of which two are implemented
for ArangoSearch Views, probably the most famous schemes:

- [Okapi BM25](https://en.wikipedia.org/wiki/Okapi_BM25){:target="_blank"}
- [TF-IDF](https://en.wikipedia.org/wiki/Tf%E2%80%93idf){:target="_blank"}

Under the hood both models rely on two main components:
- *Term frequency* (TF): in the simplest case defined as the number of times
  that term `t` occurs in document `d`
- *Inverse document frequency* (IDF): a measure of how much information the
  word provides, i.e. whether the term is common or rare across all documents

The searching and ranking capabilities are provided by the
[IResearch library](https://github.com/iresearch-toolkit/iresearch){:target="_blank"}.

Integration
-----------

The collections to act as data source need to be _linked_ to a View.
An ArangoSearch Link is a uni-directional connection from an ArangoDB collection
to an ArangoSearch View describing how data coming from the said collection
should be made available in the given View. You can think of it as a data flow
from a collection to a View. A View can have zero or more links, each to a
distinct ArangoDB collection within a database. The same collections may be
linked to other Views too.

ArangoSearch Views are not updated synchronously as the source collections
change in order to minimize the performance impact. They are eventually
consistent, with a configurable consolidation policy.

Document as well as edge collections can be linked, which means graphs can be
treated as flat and interconnected data structure simultaneously. For example,
one can find the most relevant vertices by searching and sorting via a View,
then do a regular traversal within a specified depth.

Links can be managed by editing the [View Definition](#view-definitionmodification).
It is possible to index all attributes or particular attributes (optionally
including nested attributes). Any document attribute at any depth can be
indexed. A list of Analyzers to process the values with can be defined for each
such field.

The Analyzer(s) defined in the View and the one(s) specified in a query must
match to produce a result. For example, if a field was only indexed using the
`"identity"` Analyzer but the search expression compares the value to something
using a different Analyzer (e.g. `"text_en"`) then nothing is found.

The elements of arrays are indexed individually by default, as if the source
attribute had each element as value at the same time. Strings may get
transformed by Analyzers into multiple tokens, which are handled similarly to
an array of strings. See
[AQL SEARCH operation](aql/operations-search.html#arrays-and-tracklistpositions)
for details. Primitive values other than strings (`null`, `true`, `false`,
numbers) are indexed unchanged. The values of nested object are optionally
indexed under the respective attribute path, including objects in arrays.

Views can be managed in the Web UI, via an [HTTP API](http/views.html) and
through a [JavaScript API](data-modeling-views-database-methods.html).

Finally, Views can be queried with AQL via the
[SEARCH operation](aql/operations-search.html).

Primary Sort Order
------------------

The index behind an ArangoSearch View can have a primary sort order.
A direction can be specified upon View creation for each uniquely named
attribute (ascending or descending), to enable an optimization for AQL
queries which iterate over a View and sort by one or multiple of the
attributes. If the field(s) and the sorting direction(s) match then the
the data can be read directly from the index without actual sort operation.

View definition example:

```json
{
  "links": {
    "coll1": {
      "fields": {
        "text": {
        }
      }
    },
    "coll2": {
      "fields": {
        "text": {
      }
    }
  },
  "primarySort": [
    {
      "field": "text",
      "direction": "asc"
    }
  ]
}
```

AQL query example:

```js
FOR doc IN viewName
  SORT doc.text
  RETURN doc
```

Execution plan **without** a sorted index being used:

```
Execution plan:
 Id   NodeType            Est.   Comment
  1   SingletonNode          1   * ROOT
  2   EnumerateViewNode      1     - FOR doc IN viewName   /* view query */
  3   CalculationNode        1       - LET #1 = doc.`val`   /* attribute expression */
  4   SortNode               1       - SORT #1 ASC   /* sorting strategy: standard */
  5   ReturnNode             1       - RETURN doc
```

Execution plan with a the primary sort order of the index being utilized:

```
Execution plan:
 Id   NodeType            Est.   Comment
  1   SingletonNode          1   * ROOT
  2   EnumerateViewNode      1     - FOR doc IN viewName SORT doc.`val` ASC   /* view query */
  5   ReturnNode             1       - RETURN doc
```

To define more than one attribute to sort by, simply add more sub-objects to
the `primarySort` array:

```json
  "primarySort": [
    {
      "field": "date",
      "direction": "desc"
    },
    {
      "field": "text",
      "direction": "asc"
    }
  ]
```

The optimization can be applied to View queries which sort by both fields as
defined (`SORT doc.date DESC, doc.text`), but also if they sort in descending
order by the `date` attribute only (`SORT doc.date DESC`). Queries which sort
by `text` alone (`SORT doc.text`) are not eligible, because the View is sorted
by `date` first. This is similar to skiplist indexes, but inverted sorting
directions are not covered by the View index
(e.g. `SORT doc.date, doc.text DESC`).

Note that the `primarySort` option is immutable: it can not be changed after
View creation. It is therefore not possible to configure it through the Web UI.
The View needs to be created via the HTTP or JavaScript API (arangosh) to set it.

View Definition/Modification
----------------------------

An ArangoSearch View is configured via an object containing a set of
View-specific configuration directives and a map of link-specific configuration
directives.

During View creation the following directives apply:

- **name** (string, _immutable_): the View name
- **type** (string, _immutable_): the value `"arangosearch"`
- any of the directives from the section [View Properties](#view-properties)

During view modification the following directives apply:

- **links** (object, _optional_):
  a mapping of `collection-name` / `collection-identifier` to one of:
  - link creation - link definition as per the section [Link properties](#link-properties)
  - link removal - JSON keyword *null* (i.e. nullify a link if present)
- any of the directives from the section [View Properties](#view-properties)

### Link Properties

- **analyzers** (_optional_; type: `array`; subtype: `string`; default: `[
  'identity' ]`)

  A list of analyzers, by name as defined via the [Analyzers](arangosearch-analyzers.html),
  that should be applied to values of processed document attributes.

- **fields** (_optional_; type: `object`; default: `{}`)

  An object `{ attribute-name: [Link properties], … }` of fields that should be
  processed at each level of the document. Each key specifies the document
  attribute to be processed. Note that the value of `includeAllFields` is also
  consulted when selecting fields to be processed. It is a recursive data
  structure. Each value specifies the [Link properties](#link-properties)
  directives to be used when processing the specified field, a Link properties
  value of `{}` denotes inheritance of all (except `fields`) directives from
  the current level.

- **includeAllFields** (_optional_; type: `boolean`; default: `false`)

  If set to `true`, then process all document attributes. Otherwise, only
  consider attributes mentioned in `fields`. Attributes not explicitly
  specified in `fields` will be processed with default link properties, i.e.
  `{}`.

- **trackListPositions** (_optional_; type: `boolean`; default: `false`)

  If set to `true`, then for array values track the value position in arrays.
  E.g., when querying for the input `{ attr: [ 'valueX', 'valueY', 'valueZ' ]
  }`, the user must specify: `doc.attr[1] == 'valueY'`. Otherwise, all values in
  an array are treated as equal alternatives. E.g., when querying for the input
  `{ attr: [ 'valueX', 'valueY', 'valueZ' ] }`, the user must specify: `doc.attr
  == 'valueY'`.

- **storeValues** (_optional_; type: `string`; default: `"none"`)

  This property controls how the view should keep track of the attribute values.
  Valid values are:

  - **none**: Do not store values with the view.
  - **id**: Store information about value presence to allow use of the
    `EXISTS()` function.

### View Properties

- **primarySort** (_optional_; type: `array`; default: `[]`; _immutable_)

  A primary sort order can be defined to enable an AQL optimization. If a query
  iterates over all documents of a View, wants to sort them by attribute values
  and the (left-most) fields to sort by as well as their sorting direction match
  with the *primarySort* definition, then the `SORT` operation is optimized away.
  Also see [Primary Sort Order](arangosearch-views.html#primary-sort-order)

An inverted index is the heart of ArangoSearch Views.
The index consists of several independent segments and the index **segment**
itself is meant to be treated as a standalone index. **Commit** is meant to be
treated as the procedure of accumulating processed data creating new index
segments. **Consolidation** is meant to be treated as the procedure of joining
multiple index segments into a bigger one and removing garbage documents (e.g.
deleted from a collection). **Cleanup** is meant to be treated as the procedure
of removing unused segments after release of internal resources.

- **cleanupIntervalStep** (_optional_; type: `integer`; default: `2`; to
  disable use: `0`)

  ArangoSearch waits at least this many commits between removing unused files in
  its data directory for the case where the consolidation policies merge
  segments often (i.e. a lot of commit+consolidate). A lower value will cause a
  lot of disk space to be wasted for the case where the consolidation policies
  rarely merge segments (i.e. few inserts/deletes). A higher value will impact
  performance without any added benefits.

  > With every **commit** or **consolidate** operation a new state of the view
  > internal data-structures is created on disk. Old states/snapshots are
  > released once there are no longer any users remaining. However, the files
  > for the released states/snapshots are left on disk, and only removed by
  > "cleanup" operation.

- **commitIntervalMsec** (_optional_; type: `integer`; default: `1000`;
  to disable use: `0`)

  Wait at least this many milliseconds between committing View data store
  changes and making documents visible to queries (default: 1000, to disable
  use: 0).
  For the case where there are a lot of inserts/updates, a lower value, until
  commit, will cause the index not to account for them and memory usage would
  continue to grow.
  For the case where there are a few inserts/updates, a higher value will impact
  performance and waste disk space for each commit call without any added
  benefits.

  > For data retrieval ArangoSearch Views follow the concept of
  > "eventually-consistent", i.e. eventually all the data in ArangoDB will be
  > matched by corresponding query expressions.
  > The concept of ArangoSearch View "commit" operation is introduced to
  > control the upper-bound on the time until document addition/removals are
  > actually reflected by corresponding query expressions.
  > Once a "commit" operation is complete all documents added/removed prior to
  > the start of the "commit" operation will be reflected by queries invoked in
  > subsequent ArangoDB transactions, in-progress ArangoDB transactions will
  > still continue to return a repeatable-read state.

- **consolidationIntervalMsec** (_optional_; type: `integer`; default: `60000`;
  to disable use: `0`)

  ArangoSearch waits _at least_ this many milliseconds between committing view
  data store changes and making documents visible to queries. A lower value
  will cause the view not to account for them, (until commit), and memory usage
  would continue to grow for the case where there are a few inserts/updates. A
  higher value will impact performance and waste disk space for each commit call
  without any added benefits.

  > For data retrieval ArangoSearch Views follow the concept of
  > "eventually-consistent", i.e. eventually all the data in ArangoDB will be
  > matched by corresponding query expressions. The concept of an ArangoSearch
  > View "commit" operation is introduced to control the upper-bound on the time
  > until document addition/removals are actually reflected by corresponding
  > query expressions. Once a **commit** operation is complete, all documents
  > added/removed prior to the start of the **commit** operation will be
  > reflected by queries invoked in subsequent ArangoDB transactions, while
  > in-progress ArangoDB transactions will still continue to return a
  > repeatable-read state.

ArangoSearch performs operations in its index based on numerous writer
objects that are mapped to processed segments. In order to control memory that
is used by these writers (in terms of "writers pool") one can use
`writebuffer*` properties of a view.

- **writebufferIdle** (_optional_; type: `integer`; default: `64`;
  to disable use: `0`; _immutable_)

  Maximum number of writers (segments) cached in the pool.

- **writebufferActive** (_optional_; type: `integer`; default: `0`;
  to disable use: `0`; _immutable_)

  Maximum number of concurrent active writers (segments) that perform a transaction.
  Other writers (segments) wait till current active writers (segments) finish.

- **writebufferSizeMax** (_optional_; type: `integer`; default: `33554432`;
  to disable use: `0`; _immutable_)

  Maximum memory byte size per writer (segment) before a writer (segment) flush is
  triggered. `0` value turns off this limit for any writer (buffer) and data will
  be flushed periodically based on the
  [value defined for the flush thread](programs-arangod-server.html#data-source-flush-synchronization)
  (ArangoDB server startup option). `0` value should be used carefully due to high
  potential memory consumption.

- **consolidationPolicy** (_optional_; type: `object`; default: `{}`)

  The consolidation policy to apply for selecting data store segment merge
  candidates.

  > With each ArangoDB transaction that inserts documents, one or more
  > ArangoSearch internal segments gets created. Similarly, for removed
  > documents the segments containing such documents will have these documents
  > marked as "deleted". Over time this approach causes a lot of small and
  > sparse segments to be created. A **consolidation** operation selects one or
  > more segments and copies all of their valid documents into a single new
  > segment, thereby allowing the search algorithm to perform more optimally and
  > for extra file handles to be released once old segments are no longer used.

  - **type** (_optional_; type: `string`; default: `"bytes_accum"`)

    The segment candidates for the "consolidation" operation are selected based
    upon several possible configurable formulas as defined by their types.
    The currently supported types are:

    - `"bytes_accum"`: Consolidation is performed based on current memory
      consumption of segments and `threshold` property value.
    - `"tier"`: Consolidate based on segment byte size and live document count
      as dictated by the customization attributes.

  `consolidationPolicy` properties for `"bytes_accum"` type:

  - **threshold** (_optional_; type: `float`; default: `0.1`)

    Defines threshold value of `[0.0, 1.0]` possible range. Consolidation is
    performed on segments which accumulated size in bytes is less than all
    segments' byte size multiplied by the `threshold`; i.e. the following formula
    is applied for each segment:
    `{threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes`.

  `consolidationPolicy` properties for `"tier"` type:

  - **segmentsMin** (_optional_; type: `integer`; default: `1`)

    The minimum number of segments that will be evaluated as candidates for consolidation.

  - **segmentsMax** (_optional_; type: `integer`; default: `10`)

    The maximum number of segments that will be evaluated as candidates for consolidation.

  - **segmentsBytesMax** (_optional_; type: `integer`; default: `5368709120`)

    Maximum allowed size of all consolidated segments in bytes.

  - **segmentsBytesFloor** (_optional_; type: `integer`; default: `2097152`)

    Defines the value (in bytes) to treat all smaller segments as equal for consolidation
    selection.
