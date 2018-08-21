# Bringing the power of IResearch to ArangoDB

## What is ArangoSearch

ArangoSearch is a natively integrated AQL extension making use of the
[IResearch library](https://github.com/iresearch-toolkit/iresearch).

Arangosearch allows one to:
* join documents located in different collections to one result list
* search documents based on AQL boolean expressions and functions
* sort the result set based on how closely each document matched the search condition

A concept of value 'analysis' that is meant to break up a given value into
a set of sub-values internally tied together by metadata which influences both
the search and sort stages to provide the most appropriate match for the
specified conditions, similar to queries to web search engines.

In plain terms this means a user can for example:
* request documents where the 'body' attribute best matches 'a quick brown fox'
* request documents where the 'dna' attribute best matches a DNA sub sequence
* request documents where the 'name' attribute best matches gender
* etc... (via custom analyzers described in the next section)

### The IResearch Library

IResearch s a cross-platform open source indexing and searching engine written in C++,
optimized for speed and memory footprint, with source available from:
https://github.com/iresearch-toolkit/iresearch

IResearch is a framework for indexing, searching and sorting of data. The indexing stage can
treat each data item as an atom or use custom 'analyzers' to break the data item
into sub-atomic pieces tied together with internally tracked metadata.

The IResearch framework in general can be further extended at runtime with
custom implementations of analyzers (used during the indexing and searching
stages) and scorers (used during the sorting stage) allowing full control over
the behavior of the engine.


### ArangoSearch Scorers

ArangoSearch accesses scorers directly by their internal names. The
name (in upper-case) of the scorer is the function name to be used in the
['SORT' section](../../../AQL/Views/ArangoSearch/index.html#arangosearch-sort).
Function arguments, (excluding the first argument), are serialized as a
string representation of a JSON array and passed directly to the corresponding
scorer. The first argument to any scorer function is the reference to the 
current document emitted by the `FOR` statement, i.e. it would be 'doc' for this
statement:

    FOR doc IN someView

IResearch provides a 'bm25' scorer implementing the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25). This scorer
optionally takes 'k' and 'b' positional parameters.

The user is able to run queries with the said scorer, e.g.

    SORT BM25(doc, 1.2, 0.75)

The function arguments will then be serialized into a JSON representation:

```json
[ 1.2, 0.75 ]
```

and passed to the scorer implementation.

Similarly an administrator may choose to deploy a custom DNA analyzer 'DnaRank'.

The user is then immediately able to run queries with the said scorer, e.g.

    SORT DNARANK(doc, 123, 456, "abc", { "def", "ghi" })

The function arguments will then be serialized into a JSON representation:

```json
[ 123, 456, "abc", { "def", "ghi" } ]
```

and passed to the scorer implementation.

Runtime-plugging functionality for scores is not available in ArangoDB at this
point in time, so ArangoDB comes with a few default-initialized scores:

- *attribute-name*
  order results based on the value of **attribute-name**

- BM25
  order results based on the
  [BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

- TFIDF
  order results based on the
  [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)

### ArangoSearch is much more than a fulltext search

But fulltext searching is a subset of its available functionality, supported via
the 'text' analyzer and 'tfidf'/'bm25' scorers, without impact to performance
when specifying documents from different collections or searching on multiple
document attributes.

### View datasource

The IResearch functionality is exposed to ArangoDB via the view API for views
of type *arangosearch*, because the ArangoSearch view is merely an identity
transformation applied onto documents stored in linked collections of the same
ArangoDB database.
In plain terms an ArangoSearch view only allows searching and sorting of documents
located in collections of the same database.
The matching documents themselves are returned as-is from their corresponding collections.

### Links to ArangoDB collections

A concept of an ArangoDB collection 'link' is introduced to allow specifying
which ArangoDB collections a given ArangoSearch View should query for documents
and how these documents should be queried.

An ArangoSearch Link is a uni-directional connection from an ArangoDB collection
to an ArangoSearch view describing how data coming from the said collection should
be made available in the given view. Each ArangoSearch Link in an ArangoSearch view is
uniquely identified by the name of the ArangoDB collection it links to. An
ArangoSearch view may have zero or more links, each to a distinct ArangoDB
collection. Similarly an ArangoDB collection may be referenced via links by zero
or more distinct ArangoSearch views. In plain terms any given ArangoSearch view may be
linked to any given ArangoDB collection of the same database with zero or at
most one link. However, any ArangoSearch view may be linked to multiple distinct
ArangoDB collections and similarly any ArangoDB collection may be referenced by
multiple ArangoSearch views.

To configure an ArangoSearch view for consideration of documents from a given
ArangoDB collection a link definition must be added to the properties of the
said ArangoSearch view defining the link parameters as per the section
[View definition/modification](#view-definitionmodification).

### Analyzers

To simplify query syntax ArangoSearch provides a concept of 
[named analyzers](Analyzers.md) which
are merely aliases for type+configuration of IResearch analyzers. Management of
named analyzers is exposed via both REST, GUI and JavaScript APIs, e.g.


### View definition/modification

An ArangoSearch view is configured via an object containing a set of
view-specific configuration directives and a map of link-specific configuration
directives.

During view creation the following directives apply:
* id: (optional) the desired view identifier
* name: (required) the view name
* type: (required) the value "arangosearch"
  any of the directives from the section [View properties](#view-properties-updatable)

During view modification the following directives apply:
* links: (optional)
  a mapping of collection-name/collection-identifier to one of:
  * link creation - link definition as per the section [Link properties](#link-properties)
  * link removal - JSON keyword *null* (i.e. nullify a link if present)
    any of the directives from the section [modifiable view properties](#view-properties-updatable)

### View properties (non-updatable)

* locale: (optional; default: `C`)
  the locale/collation used for ordering processed attribute names within
  ArangoSearch internal structures
  the value format is any string representation of a locale name that is
  recognised by Boost and ICU can understand, e.g.
  {Language}_{Country}.{Encoding}@{Variant}


### View properties (updatable)

* cleanupIntervalStep: (optional; default: `10`; to disable use: `0`)
  wait at least this many commits between removing unused files in the
  ArangoSearch data directory
  for the case where the consolidation policies merge segments often (i.e. a
  lot of commit+consolidate), a lower value will cause a lot of disk space to
  be wasted
  for the case where the consolidation policies rarely merge segments (i.e.
  few inserts/deletes), a higher value will impact performance without any
  added benefits
  background:
    with every "commit" or "consolidate" operation a new state of the view
    internal data-structures is created on disk
    old states/snapshots are released once there are no longer any users
    remaining
    however, the files for the released states/snapshots are left on disk, and
    only removed by "cleanup" operation

* commitIntervalMsec: (optional; default: `60000`; to disable use: `0`)
  wait at least this many milliseconds between committing view data store
  changes and making documents visible to queries
  for the case where there are a lot of inserts/updates, a lower value will
  cause the view not to account for them, (unlit commit), and memory usage
  would continue to grow
  for the case where there are a few inserts/updates, a higher value will
  impact performance and waste disk space for each commit call without any
  added benefits
  background:
    for data retrieval ArangoSearch views follow the concept of
    "eventually-consistent", i.e. eventually all the data in ArangoDB will be
    matched by corresponding query expressions
    the concept of ArangoSearch view "commit" operation is introduced to
    control the upper-bound on the time until document addition/removals are
    actually reflected by corresponding query expressions
    once a "commit" operation is complete all documents added/removed prior to
    the start of the "commit" operation will be reflected by queries invoked in
    subsequent ArangoDB transactions, in-progress ArangoDB transactions will
    still continue to return a repeatable-read state

* consolidate: (optional; default: `{}`; to disable use: `null`)
  the consolidation policy to apply for selecting data store segment merge
  candidates
  background:
    with each ArangoDB transaction that inserts documents one or more
    ArangoSearch internal segments gets created
    similarly for removed documents the segments that contain such documents
    will have these documents marked as 'deleted'
    over time this approach causes a lot of small and sparse segments to be
    created
    a "consolidation" operation selects one or more segments and copies all of
    their valid documents into a single new segment, thereby allowing the
    search algorithm to perform more optimally and for extra file handles to be
    released once old segments are no longer used

  * type: (optional; default "bytes_accum")
    the segment candidates for the "consolidation" operation are selected based
    upon several possible configurable formulas as defined by their types
    the currently supported types are:
    - *bytes*: consolidate if and only if:
      {threshold} > segment_bytes / (all_segment_bytes / number_of_segments)
      i.e. the candidate segment byte size is less that the average segment
           byte size multiplied by the {threshold}
    - *bytes_accum*: consolidate if and only if:
      {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
      i.e. the sum of all candidate segment byte size is less than the total
           segment byte size multiplied by the {threshold}
    - *count*: consolidate if and only if:
      {threshold} > segment_docs{valid} / (all_segment_docs{valid} / number_of_segments)
      i.e. the candidate segment non-deleted document count is less that the
           average segment non-deleted document count size multiplied by the
           {threshold}
    - *fill*: consolidate if and only if:
      {threshold} > #segment_docs{valid} / (#segment_docs{valid} + number_of_segment_docs{removed})
      i.e. the candidate segment valid document count is less that the average
           segment total document count multiplied by the {threshold}

  * segmentThreshold: (optional, default: `300`)
    apply the "consolidation" operation if and only if:
    {segmentThreshold} < number_of_segments

  * threshold: (optional; default: `0.85`)
    select a given segment for "consolidation" if and only if the formula based
    on *type* (as defined above) evaluates to true, valid value range
    `[0.0, 1.0]`


### Link properties

* analyzers: (optional; default: `[ 'identity' ]`)
  a list of analyzers, by name as defined via the [Analyzers](Analyzers.md), that
  should be applied to values of processed document attributes

* fields: (optional; default: `{}`)
  an object `{attribute-name: [Link properties]}` of fields that should be
  processed at each level of the document
  each key specifies the document attribute to be processed, the value of
  *includeAllFields* is also consulted when selecting fields to be processed
  each value specifies the [Link properties](#link-properties) directives to be used when
  processing the specified field, a Link properties value of `{}` denotes
  inheritance of all (except *fields*) directives from the current level

* includeAllFields: (optional; default: `false`)
  if true then process all document attributes (if not explicitly specified
  then process the fields with default Link properties directives, i.e. `{}`),
  otherwise only consider attributes mentioned in *fields*

* trackListPositions: (optional; default: false)
  if true then for array values track the value position in the array, e.g. when
  querying for the input: `{ attr: [ 'valueX', 'valueY', 'valueZ' ] }`
  the user must specify: `doc.attr[1] == 'valueY'`
  otherwise all values in an array are treated as equal alternatives, e.g. when
  querying for the input: `{ attr: [ 'valueX', 'valueY', 'valueZ' ] }`
  the user must specify: `doc.attr == 'valueY'`

* storeValues: (optional; default: "none")
  how should the view track the attribute values, this setting allows for
  additional value retrieval optimizations, one of:
  * none: Do not store values by the view
  * id: Store only information about value presence, to allow use of the EXISTS() function
