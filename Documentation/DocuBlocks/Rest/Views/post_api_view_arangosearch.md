@startDocuBlock post_api_view_arangosearch
@brief creates an ArangoSearch View

@RESTHEADER{POST /_api/view#arangosearch, Create an ArangoSearch View, createView}

@RESTBODYPARAM{name,string,required,string}
The name of the View.

@RESTBODYPARAM{type,string,required,string}
The type of the View. Must be equal to *"arangosearch"*.
This option is immutable.

@RESTBODYPARAM{links,object,optional,}
Expects an object with the attribute keys being names of to be linked collections,
and the link properties as attribute values. See
[ArangoSearch View Link Properties](https://www.arangodb.com/docs/stable/arangosearch-views.html#link-properties)
for details.

@RESTBODYPARAM{primarySort,array,optional,object}
A primary sort order can be defined to enable an AQL optimization. If a query
iterates over all documents of a View, wants to sort them by attribute values
and the (left-most) fields to sort by as well as their sorting direction match
with the *primarySort* definition, then the `SORT` operation is optimized away.
This option is immutable.<br/>
Expects an array of objects, each specifying a field (attribute path) and a
sort direction (`"asc` for ascending, `"desc"` for descending):
`[ { "field": "attr", "direction": "asc"}, … ]`

@RESTBODYPARAM{primarySortCompression,string,optional,string}
Defines how to compress the primary sort data (introduced in v3.7.1).
ArangoDB v3.5 and v3.6 always compress the index using LZ4.
This option is immutable.
- `"lz4"` (default): use LZ4 fast compression.
- `"none"`: disable compression to trade space for speed.

@RESTBODYPARAM{storedValues,array,optional,object}
An array of objects to describe which document attributes to store in the View
index (introduced in v3.7.1). It can then cover search queries, which means the
data can be taken from the index directly and accessing the storage engine can
be avoided.<br/>
Each object is expected in the form
`{ "fields": [ "attr1", "attr2", ... "attrN" ], "compression": "none" }`,
where the required `fields` attribute is an array of strings with one or more
document attribute paths. The specified attributes are placed into a single
column of the index. A column with all fields that are involved in common
search queries is ideal for performance. The column should not include too many
unneeded fields however. The optional `compression` attribute defines the
compression type used for the internal column-store, which can be `"lz4"`
(LZ4 fast compression, default) or `"none"` (no compression).<br/>
This option is immutable. Not to be confused with `storeValues`, which allows
to store meta data about attribute values in the View index.

@RESTBODYPARAM{cleanupIntervalStep,integer,optional,int64}
Wait at least this many commits between removing unused files in the
ArangoSearch data directory (default: 2, to disable use: 0).
For the case where the consolidation policies merge segments often (i.e. a lot
of commit+consolidate), a lower value will cause a lot of disk space to be
wasted.
For the case where the consolidation policies rarely merge segments (i.e. few
inserts/deletes), a higher value will impact performance without any added
benefits.<br/>
_Background:_
  With every "commit" or "consolidate" operation a new state of the View
  internal data-structures is created on disk.
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
benefits.<br/>
_Background:_
  For data retrieval ArangoSearch Views follow the concept of
  "eventually-consistent", i.e. eventually all the data in ArangoDB will be
  matched by corresponding query expressions.
  The concept of ArangoSearch View "commit" operation is introduced to
  control the upper-bound on the time until document addition/removals are
  actually reflected by corresponding query expressions.
  Once a "commit" operation is complete all documents added/removed prior to
  the start of the "commit" operation will be reflected by queries invoked in
  subsequent ArangoDB transactions, in-progress ArangoDB transactions will
  still continue to return a repeatable-read state.

@RESTBODYPARAM{consolidationIntervalMsec,integer,optional,int64}
Wait at least this many milliseconds between applying 'consolidationPolicy' to
consolidate View data store and possibly release space on the filesystem
(default: 10000, to disable use: 0).
For the case where there are a lot of data modification operations, a higher
value could potentially have the data store consume more space and file handles.
For the case where there are a few data modification operations, a lower value
will impact performance due to no segment candidates available for
consolidation.<br/>
_Background:_
  For data modification ArangoSearch Views follow the concept of a
  "versioned data store". Thus old versions of data may be removed once there
  are no longer any users of the old data. The frequency of the cleanup and
  compaction operations are governed by 'consolidationIntervalMsec' and the
  candidates for compaction are selected via 'consolidationPolicy'.

@RESTBODYPARAM{consolidationPolicy,object,optional,}
The consolidation policy to apply for selecting which segments should be merged
(default: {})<br/>
_Background:_
  With each ArangoDB transaction that inserts documents one or more
  ArangoSearch internal segments gets created.
  Similarly for removed documents the segments that contain such documents
  will have these documents marked as 'deleted'.
  Over time this approach causes a lot of small and sparse segments to be
  created.
  A "consolidation" operation selects one or more segments and copies all of
  their valid documents into a single new segment, thereby allowing the
  search algorithm to perform more optimally and for extra file handles to be
  released once old segments are no longer used.<br/>
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
      name: "testViewBasics",
      type: "arangosearch"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);

    db._flushCache();
    db._dropView("testViewBasics");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
