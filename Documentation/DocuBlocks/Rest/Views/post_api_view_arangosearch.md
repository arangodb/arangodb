@startDocuBlock post_api_view_arangosearch
@brief Creates an `arangosearch` View

@RESTHEADER{POST /_api/view, Create an arangosearch View, createView}

@RESTBODYPARAM{name,string,required,string}
The name of the View.

@RESTBODYPARAM{type,string,required,string}
The type of the View. Must be equal to `"arangosearch"`.
This option is immutable.

@RESTBODYPARAM{links,object,optional,}
Expects an object with the attribute keys being names of to be linked collections,
and the link properties as attribute values. See
[`arangosearch` View Link Properties](https://www.arangodb.com/docs/stable/arangosearch-views.html#link-properties)
for details.

@RESTBODYPARAM{primarySort,array,optional,object}
A primary sort order can be defined to enable an AQL optimization. If a query
iterates over all documents of a View, wants to sort them by attribute values
and the (left-most) fields to sort by as well as their sorting direction match
with the `primarySort` definition, then the `SORT` operation is optimized away.
This option is immutable.

Expects an array of objects, each specifying a field (attribute path) and a
sort direction (`"asc` for ascending, `"desc"` for descending):
`[ { "field": "attr", "direction": "asc"}, … ]`

@RESTBODYPARAM{primarySortCompression,string,optional,string}
Defines how to compress the primary sort data (introduced in v3.7.1).
ArangoDB v3.5 and v3.6 always compress the index using LZ4.

This option is immutable.

- `"lz4"` (default): use LZ4 fast compression.
- `"none"`: disable compression to trade space for speed.

@RESTBODYPARAM{primarySortCache,boolean,optional,}
If you enable this option, then the primary sort columns are always cached in
memory (introduced in v3.9.6, Enterprise Edition only). This can improve the
performance of queries that utilize the primary sort order. Otherwise, these
values are memory-mapped and it is up to the operating system to load them from
disk into memory and to evict them from memory.

This option is immutable.

See the `--arangosearch.columns-cache-limit` startup option to control the
memory consumption of this cache. You can reduce the memory usage of the column
cache in cluster deployments by only using the cache for leader shards, see the
`--arangosearch.columns-cache-only-leader` startup option (introduced in v3.10.6).

@RESTBODYPARAM{primaryKeyCache,boolean,optional,}
If you enable this option, then the primary key columns are always cached in
memory (introduced in v3.9.6, Enterprise Edition only). This can improve the
performance of queries that return many documents. Otherwise, these values are
memory-mapped and it is up to the operating system to load them from disk into
memory and to evict them from memory.

This option is immutable.

See the `--arangosearch.columns-cache-limit` startup option to control the
memory consumption of this cache. You can reduce the memory usage of the column
cache in cluster deployments by only using the cache for leader shards, see the
`--arangosearch.columns-cache-only-leader` startup option (introduced in v3.10.6).

@RESTBODYPARAM{optimizeTopK,array,optional,string}
An array of strings defining sort expressions that you want to optimize.
This is also known as _WAND optimization_.

If you query a View with the `SEARCH` operation in combination with a
`SORT` and `LIMIT` operation, search results can be retrieved faster if the
`SORT` expression matches one of the optimized expressions.

Only sorting by highest rank is supported, that is, sorting by the result
of a scoring function in descending order (`DESC`). Use `@doc` in the expression
where you would normally pass the document variable emitted by the `SEARCH`
operation to the scoring function.

You can define up tp 64 expressions per View.

Example: `["BM25(@doc) DESC", "TFIDF(@doc, true) DESC"]`

Default: `[]`

This property is available in the Enterprise Edition only.

@RESTBODYPARAM{storedValues,array,optional,object}
An array of objects to describe which document attributes to store in the View
index (introduced in v3.7.1). It can then cover search queries, which means the
data can be taken from the index directly and accessing the storage engine can
be avoided.

This option is immutable.

Each object is expected in the following form:

`{ "fields": [ "attr1", "attr2", ... "attrN" ], "compression": "none", "cache": false }`

- The required `fields` attribute is an array of strings with one or more
  document attribute paths. The specified attributes are placed into a single
  column of the index. A column with all fields that are involved in common
  search queries is ideal for performance. The column should not include too
  many unneeded fields, however.

- The optional `compression` attribute defines the compression type used for
  the internal column-store, which can be `"lz4"` (LZ4 fast compression, default)
  or `"none"` (no compression).

- The optional `cache` attribute allows you to always cache stored values in
  memory (introduced in v3.9.5, Enterprise Edition only). This can improve
  the query performance if stored values are involved. Otherwise, these values
  are memory-mapped and it is up to the operating system to load them from disk
  into memory and to evict them from memory.

  See the `--arangosearch.columns-cache-limit` startup option
  to control the memory consumption of this cache. You can reduce the memory
  usage of the column cache in cluster deployments by only using the cache for
  leader shards, see the `--arangosearch.columns-cache-only-leader` startup
  option (introduced in v3.10.6).

  You may use the following shorthand notations on View creation instead of
  an array of objects as described above. The default compression and cache
  settings are used in this case:

  - An array of strings, like `["attr1", "attr2"]`, to place each attribute into
    a separate column of the index (introduced in v3.10.3).

  - An array of arrays of strings, like `[["attr1", "attr2"]]`, to place the
    attributes into a single column of the index, or `[["attr1"], ["attr2"]]`
    to place each attribute into a separate column. You can also mix it with the
    full form:

  ```json
  [
    ["attr1"],
    ["attr2", "attr3"],
    { "fields": ["attr4", "attr5"], "cache": true }
  ]
  ```

The `storedValues` option is not to be confused with the `storeValues` option,
which allows to store meta data about attribute values in the View index.

@RESTBODYPARAM{cleanupIntervalStep,integer,optional,int64}
Wait at least this many commits between removing unused files in the
ArangoSearch data directory (default: 2, to disable use: 0).
For the case where the consolidation policies merge segments often (i.e. a lot
of commit+consolidate), a lower value will cause a lot of disk space to be
wasted.
For the case where the consolidation policies rarely merge segments (i.e. few
inserts/deletes), a higher value will impact performance without any added
benefits.

_Background:_
  With every "commit" or "consolidate" operation a new state of the View's
  internal data structures is created on disk.
  Old states/snapshots are released once there are no longer any users
  remaining.
  However, the files for the released states/snapshots are left on disk, and
  only removed by "cleanup" operation.

@RESTBODYPARAM{commitIntervalMsec,integer,optional,int64}
Wait at least this many milliseconds between committing View data store
changes and making documents visible to queries (default: 1000, to disable
use: 0).
For the case where there are a lot of inserts/updates, a lower value, until
commit, will cause the index not to account for them and memory usage would
continue to grow.
For the case where there are a few inserts/updates, a higher value will impact
performance and waste disk space for each commit call without any added
benefits.

_Background:_
  For data retrieval, ArangoSearch follows the concept of
  "eventually-consistent", i.e. eventually all the data in ArangoDB will be
  matched by corresponding query expressions.
  The concept of ArangoSearch "commit" operations is introduced to
  control the upper-bound on the time until document addition/removals are
  actually reflected by corresponding query expressions.
  Once a "commit" operation is complete, all documents added/removed prior to
  the start of the "commit" operation will be reflected by queries invoked in
  subsequent ArangoDB transactions, in-progress ArangoDB transactions will
  still continue to return a repeatable-read state.

@RESTBODYPARAM{consolidationIntervalMsec,integer,optional,int64}
Wait at least this many milliseconds between applying `consolidationPolicy` to
consolidate the View data store and possibly release space on the filesystem
(default: 10000, to disable use: 0).
For the case where there are a lot of data modification operations, a higher
value could potentially have the data store consume more space and file handles.
For the case where there are a few data modification operations, a lower value
will impact performance due to no segment candidates available for
consolidation.

_Background:_
  For data modification, ArangoSearch follow the concept of a
  "versioned data store". Thus old versions of data may be removed once there
  are no longer any users of the old data. The frequency of the cleanup and
  compaction operations are governed by `consolidationIntervalMsec` and the
  candidates for compaction are selected via `consolidationPolicy`.

@RESTBODYPARAM{consolidationPolicy,object,optional,}
The consolidation policy to apply for selecting which segments should be merged
(default: {})

_Background:_
  With each ArangoDB transaction that inserts documents, one or more
  ArangoSearch-internal segments get created.
  Similarly, for removed documents the segments that contain such documents
  will have these documents marked as 'deleted'.
  Over time, this approach causes a lot of small and sparse segments to be
  created.
  A "consolidation" operation selects one or more segments and copies all of
  their valid documents into a single new segment, thereby allowing the
  search algorithm to perform more optimally and for extra file handles to be
  released once old segments are no longer used.

Sub-properties:
  - `type` (string, _optional_):
    The segment candidates for the "consolidation" operation are selected based
    upon several possible configurable formulas as defined by their types.
    The currently supported types are:
    - `"tier"` (default): consolidate based on segment byte size and live
      document count as dictated by the customization attributes. If this type
      is used, then below `segments*` and `minScore` properties are available.
    - `"bytes_accum"`: consolidate if and only if
      `{threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes`
      i.e. the sum of all candidate segment byte size is less than the total
      segment byte size multiplied by the `{threshold}`. If this type is used,
      then below `threshold` property is available.
  - `threshold` (number, _optional_): value in the range `[0.0, 1.0]`
  - `segmentsBytesFloor` (number, _optional_): Defines the value (in bytes) to
    treat all smaller segments as equal for consolidation selection
    (default: 2097152)
  - `segmentsBytesMax` (number, _optional_): Maximum allowed size of all
    consolidated segments in bytes (default: 5368709120)
  - `segmentsMax` (number, _optional_): The maximum number of segments that will
    be evaluated as candidates for consolidation (default: 10)
  - `segmentsMin` (number, _optional_): The minimum number of segments that will
    be evaluated as candidates for consolidation (default: 1)
  - `minScore` (number, _optional_): (default: 0)

@RESTBODYPARAM{writebufferIdle,integer,optional,int64}
Maximum number of writers (segments) cached in the pool
(default: 64, use 0 to disable, immutable)

@RESTBODYPARAM{writebufferActive,integer,optional,int64}
Maximum number of concurrent active writers (segments) that perform a
transaction. Other writers (segments) wait till current active writers
(segments) finish (default: 0, use 0 to disable, immutable)

@RESTBODYPARAM{writebufferSizeMax,integer,optional,int64}
Maximum memory byte size per writer (segment) before a writer (segment) flush
is triggered. `0` value turns off this limit for any writer (buffer) and data
will be flushed periodically based on the value defined for the flush thread
(ArangoDB server startup option). `0` value should be used carefully due to
high potential memory consumption
(default: 33554432, use 0 to disable, immutable)

@RESTDESCRIPTION
Creates a new View with a given name and properties if it does not
already exist.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *name* or *type* attribute are missing or invalid, then an *HTTP 400*
error is returned.

@RESTRETURNCODE{409}
If a View called *name* already exists, then an *HTTP 409* error is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPostViewArangoSearch}
    var url = "/_api/view";
    var body = {
      name: "products",
      type: "arangosearch"
    };
    var response = logCurlRequest('POST', url, body);
    assert(response.code === 201);
    logJsonResponse(response);

    db._dropView("products");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
