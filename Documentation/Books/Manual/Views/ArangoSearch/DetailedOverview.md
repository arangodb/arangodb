# Detailed overview of ArangoSearch Views

ArangoSearch is a powerful fulltext search component with additional
functionality, supported via the *text* analyzer and *tfidf* / *bm25*
[scorers](Scorers.md), without impact on performance when specifying documents
from different collections or filtering on multiple document attributes.

## View datasource

Search functionality is exposed to ArangoDB via the view API for views of type
`arangosearch`. The ArangoSearch View is merely an identity transformation
applied onto documents stored in linked collections of the same ArangoDB
database. In plain terms an ArangoSearch View only allows filtering and sorting
of documents located in collections of the same database. The matching documents
themselves are returned as-is from their corresponding collections.

## Links to ArangoDB collections

A concept of an ArangoDB collection 'link' is introduced to allow specifying
which ArangoDB collections a given ArangoSearch View should query for documents
and how these documents should be queried.

An ArangoSearch Link is a uni-directional connection from an ArangoDB collection
to an ArangoSearch View describing how data coming from the said collection
should be made available in the given view. Each ArangoSearch Link in an
ArangoSearch view is uniquely identified by the name of the ArangoDB collection
it links to. An ArangoSearch View may have zero or more links, each to a
distinct ArangoDB collection. Similarly an ArangoDB collection may be referenced
via links by zero or more distinct ArangoSearch Views. In other words, any given
ArangoSearch View may be linked to any given ArangoDB collection of the same
database with zero or one link. However, any ArangoSearch View may be linked to
multiple distinct ArangoDB collections and similarly any ArangoDB collection may
be referenced by multiple ArangoSearch Views.

To configure an ArangoSearch View for consideration of documents from a given
ArangoDB collection a link definition must be added to the properties of the
said ArangoSearch View defining the link parameters as per the section
[View definition/modification](#view-definitionmodification).

## Index

Inverted Index is the heart of ArangoSearch. The index consists of several
independent segments and the index segment itself is meant to be treated as a
standalone index.

## Analyzers

To simplify query syntax ArangoSearch provides a concept of
[named analyzers](Analyzers.md) which are merely aliases for type+configuration
of IResearch analyzers.
<!-- Management of named analyzers is exposed via REST, GUI and JavaScript APIs. -->

## View definition/modification

An ArangoSearch View is configured via an object containing a set of
view-specific configuration directives and a map of link-specific configuration
directives.

During view creation the following directives apply:

- **name** (_required_; type: `string`): the view name
- **type** (_required_; type: `string`): the value `"arangosearch"`
- any of the directives from the section [View properties](#view-properties)

During view modification the following directives apply:

- **links** (_optional_; type: `object`):
  a mapping of `collection-name/collection-identifier` to one of:
  - link creation - link definition as per the section [Link properties](#link-properties)
  - link removal - JSON keyword *null* (i.e. nullify a link if present)<br/>
- any of the directives from the section [View properties](#view-properties)

## View properties
The following terminology from ArangoSearch architecture is used to understand
view properties assignment of its type:

The index consists of several independent segments and the index **segment**
itself is meant to be treated as a standalone index. **Commit** is meant to be
treated as the procedure of accumulating processed data creating new index
segments. **Consolidation** is meant to be treated as the procedure of joining
multiple index segments into a bigger one and removing garbage documents (e.g.
deleted from a collection). **Cleanup** is meant to be treated as the procedure
of removing unused segments after release of internal resources.

- **cleanupIntervalStep** (_optional_; type: `integer`; default: `10`; to
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
  to disable use: `0`)

  Maximum number of writers (segments) cached in the pool.

- **writebufferActive** (_optional_; type: `integer`; default: `0`;
  to disable use: `0`)

  Maximum number of concurrent active writers (segments) that perform a transaction.
  Other writers (segments) wait till current active writers (segments) finish.

- **writebufferSizeMax** (_optional_; type: `integer`; default: `33554432`;
  to disable use: `0`)

  Maximum memory byte size per writer (segment) before a writer (segment) flush is
  triggered. `0` value turns off this limit for any writer (buffer) and data will
  be flushed periodically based on the
  [value defined for the flush thread](../../Programs/Arangod/Server.md#data-source-flush-synchronization)
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

    - **bytes_accum**: Consolidation is performed based on current memory cunsumption
      of segments and `threshold` property value.
    - **tier**: Consolidate based on segment byte size and live document count
      as dictated by the customization attributes.

### `consolidationPolicy` properties for `bytes_accum` type
  - **threshold** (_optional_; type: `float`; default: `0.1`)

    Defines threshold value of `[0.0, 1.0]` possible range. Consolidation is
    performed on segments which accumulated size in bytes is less than all
    segments' byte size multiplied by the `threshold`; i.e. the following formula
    is applied for each segment:
    `{threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes`.

### `consolidationPolicy` properties for `tier` type

  - **segmentsMin** (_optional_; type: `integer`; default: `1`)

    The minimum number of segments that will be evaluated as candidates for consolidation.

  - **segmentsMax** (_optional_; type: `integer`; default: `10`)

    The maximum number of segments that will be evaluated as candidates for consolidation.

  - **segmentsBytesMax** (_optional_; type: `integer`; default: `5368709120`)

    Maximum allowed size of all consolidated segments in bytes.

  - **segmentsBytesFloor** (_optional_; type: `integer`; default: `2097152`)

    Defines the value (in bytes) to treat all smaller segments as equal for consolidation
    selection.

  - **lookahead** (_optional_; type: `integer`; default: `18446744073709552000`)

    The number of additionally searched tiers except initially chosen candidates based on
    `segmentsMin`, `segmentsMax`, `segmentsBytesMax`, `segmentsBytesFloor` with
    respect to defined values. Default value is treated as searching among all existing
    segments.

## Link properties

- **analyzers** (_optional_; type: `array`; subtype: `string`; default: `[
  'identity' ]`)

  A list of analyzers, by name as defined via the [Analyzers](Analyzers.md),
  that should be applied to values of processed document attributes.

- **fields** (_optional_; type: `object`; default: `{}`)

  An object `{attribute-name: [Link properties]}` of fields that should be
  processed at each level of the document. Each key specifies the document
  attribute to be processed. Note that the value of `includeAllFields` is also
  consulted when selecting fields to be processed. Each value specifies the
  [Link properties](#link-properties) directives to be used when processing the
  specified field, a Link properties value of `{}` denotes inheritance of all
  (except `fields`) directives from the current level.

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
