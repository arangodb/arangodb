
@startDocuBlock post_api_index_inverted
@brief Creates an inverted index

@RESTHEADER{POST /_api/index#inverted, Create an inverted index, createIndex:inverted}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
Must be equal to `"inverted"`.

@RESTBODYPARAM{name,string,optional,string}
An easy-to-remember name for the index to look it up or refer to it in index hints.
Index names are subject to the same character restrictions as collection names.
If omitted, a name is auto-generated so that it is unique with respect to the
collection, e.g. `idx_832910498`.

@RESTBODYPARAM{fields,array,required,post_api_index_inverted_fields}
An array of attribute paths as strings to index the fields with the default
options, or objects to specify options for the fields.

@RESTSTRUCT{name,post_api_index_inverted_fields,string,required,}
An attribute path. The `.` character denotes sub-attributes.

@RESTSTRUCT{analyzer,post_api_index_inverted_fields,string,optional,}
The name of an Analyzer to use for this field.
Default: the value defined by the top-level `analyzer` option, or if not set,
the default `identity` Analyzer.

@RESTSTRUCT{features,post_api_index_inverted_fields,array,optional,string}
A list of Analyzer features to use for this field. They define what features are
enabled for the `analyzer`. Possible features:
- `"frequency"`
- `"norm"`
- `"position"`
- `"offset"`

Default: the value of the top-level `features` option, or if not set, the
features defined by the Analyzer itself.

@RESTSTRUCT{includeAllFields,post_api_index_inverted_fields,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

If set to `true`, then all sub-attributes of this field are indexed, excluding
any sub-attributes that are configured separately by other elements in the
`fields` array (and their sub-attributes). The `analyzer` and `features`
properties apply to the sub-attributes.

If set to `false`, then sub-attributes are ignored. The default value is defined
by the top-level `includeAllFields` option, or `false` if not set.

@RESTSTRUCT{searchField,post_api_index_inverted_fields,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

You can set the option to `true` to get the same behavior as with `arangosearch`
Views regarding the indexing of array values for this field. If enabled, both,
array and primitive values (strings, numbers, etc.) are accepted. Every element
of an array is indexed according to the `trackListPositions` option.

If set to `false`, it depends on the attribute path. If it explicitly expand an
array (`[*]`), then the elements are indexed separately. Otherwise, the array is
indexed as a whole, but only `geopoint` and `aql` Analyzers accept array inputs.
You cannot use an array expansion if `searchField` is enabled.

Default: the value defined by the top-level `searchField` option, or `false` if
not set.

@RESTSTRUCT{trackListPositions,post_api_index_inverted_fields,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

If set to `true`, then track the value position in arrays for array values.
For example, when querying a document like `{ attr: [ "valueX", "valueY", "valueZ" ] }`,
you need to specify the array element, e.g. `doc.attr[1] == "valueY"`.

If set to `false`, all values in an array are treated as equal alternatives.
You don't specify an array element in queries, e.g. `doc.attr == "valueY"`, and
all elements are searched for a match.

Default: the value defined by the top-level `trackListPositions` option, or
`false` if not set.

@RESTSTRUCT{nested,post_api_index_inverted_fields,array,optional,post_api_index_inverted_nested}
Index the specified sub-objects that are stored in an array. Other than with the
`fields` property, the values get indexed in a way that lets you query for
co-occurring values. For example, you can search the sub-objects and all the
conditions need to be met by a single sub-object instead of across all of them.

This property is available in the Enterprise Edition only.

@RESTSTRUCT{name,post_api_index_inverted_nested,string,required,}
An attribute path. The `.` character denotes sub-attributes.

@RESTSTRUCT{analyzer,post_api_index_inverted_nested,string,optional,}
The name of an Analyzer to use for this field.
Default: the value defined by the top-level `analyzer` option.

@RESTSTRUCT{features,post_api_index_inverted_nested,array,optional,string}
A list of Analyzer features to use for this field. They define what features are
enabled for the `analyzer`. Possible features:
- `"frequency"`
- `"norm"`
- `"position"`
- `"offset"`

Default: the value of the top-level `features` option, or if not set, the
features defined by the Analyzer itself.

@RESTSTRUCT{searchField,post_api_index_inverted_nested,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

You can set the option to `true` to get the same behavior as with `arangosearch`
Views regarding the indexing of array values for this field. If enabled, both,
array and primitive values (strings, numbers, etc.) are accepted. Every element
of an array is indexed according to the `trackListPositions` option.

If set to `false`, it depends on the attribute path. If it explicitly expand an
array (`[*]`), then the elements are indexed separately. Otherwise, the array is
indexed as a whole, but only `geopoint` and `aql` Analyzers accept array inputs.
You cannot use an array expansion if `searchField` is enabled.

Default: the value defined by the top-level `searchField` option.

@RESTBODYPARAM{searchField,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

You can set the option to `true` to get the same behavior as with `arangosearch`
Views regarding the indexing of array values as the default. If enabled, both,
array and primitive values (strings, numbers, etc.) are accepted. Every element
of an array is indexed according to the `trackListPositions` option.

If set to `false`, it depends on the attribute path. If it explicitly expand an
array (`[*]`), then the elements are indexed separately. Otherwise, the array is
indexed as a whole, but only `geopoint` and `aql` Analyzers accept array inputs.
You cannot use an array expansion if `searchField` is enabled.

@RESTBODYPARAM{storedValues,array,optional,post_api_index_inverted_storedvalues}
The optional `storedValues` attribute can contain an array of paths to additional 
attributes to store in the index. These additional attributes cannot be used for
index lookups or for sorting, but they can be used for projections. This allows an
index to fully cover more queries and avoid extra document lookups.

@RESTSTRUCT{fields,post_api_index_inverted_storedvalues,array,required,string}
A list of attribute paths. The `.` character denotes sub-attributes.

@RESTSTRUCT{compression,post_api_index_inverted_storedvalues,string,optional,}
Defines how to compress the attribute values. Possible values:
- `"lz4"` (default): use LZ4 fast compression.
- `"none"`: disable compression to trade space for speed.

@RESTBODYPARAM{primarySort,object,optional,post_api_index_inverted_primarysort}
You can define a primary sort order to enable an AQL optimization. If a query
iterates over all documents of a collection, wants to sort them by attribute values,
and the (left-most) fields to sort by, as well as their sorting direction, match
with the `primarySort` definition, then the `SORT` operation is optimized away.

@RESTSTRUCT{fields,post_api_index_inverted_primarysort,array,required,post_api_index_inverted_primarysort_fields}
An array of the fields to sort the index by and the direction to sort each field in.

@RESTSTRUCT{field,post_api_index_inverted_primarysort_fields,string,required,}
An attribute path. The `.` character denotes sub-attributes.

@RESTSTRUCT{direction,post_api_index_inverted_primarysort_fields,string,required,}
The sorting direction. Possible values:
- `"asc` for ascending
- `"desc"` for descending

@RESTSTRUCT{compression,post_api_index_inverted_primarysort,string,optional,}
Defines how to compress the primary sort data. Possible values:
- `"lz4"` (default): use LZ4 fast compression.
- `"none"`: disable compression to trade space for speed.

@RESTBODYPARAM{analyzer,string,optional,string}
The name of an Analyzer to use by default. This Analyzer is applied to the
values of the indexed fields for which you don't define Analyzers explicitly.

@RESTBODYPARAM{features,array,optional,string}
A list of Analyzer features to use by default. They define what features are
enabled for the default `analyzer`. Possible features:
- `"frequency"`
- `"norm"`
- `"position"`
- `"offset"`

The default is an empty array.

@RESTBODYPARAM{includeAllFields,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

If set to `true`, then all document attributes are indexed, excluding any
sub-attributes that are configured in the `fields` array (and their sub-attributes).
The `analyzer` and `features` properties apply to the sub-attributes.

The default is `false`.

**Warning**: Using `includeAllFields` for a lot of attributes in combination
with complex Analyzers may significantly slow down the indexing process.

@RESTBODYPARAM{trackListPositions,boolean,optional,}
This option only applies if you use the inverted index in a `search-alias` Views.

If set to `true`, then track the value position in arrays for array values.
For example, when querying a document like `{ attr: [ "valueX", "valueY", "valueZ" ] }`,
you need to specify the array element, e.g. `doc.attr[1] == "valueY"`.

If set to `false`, all values in an array are treated as equal alternatives.
You don't specify an array element in queries, e.g. `doc.attr == "valueY"`, and
all elements are searched for a match.

@RESTBODYPARAM{parallelism,integer,optional,}
The number of threads to use for indexing the fields. Default: `2`

@RESTBODYPARAM{inBackground,boolean,optional,}
This attribute can be set to `true` to create the index
in the background, not write-locking the underlying collection for
as long as if the index is built in the foreground. The default value is `false`.

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
benefits.

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
(default: 1000, to disable use: 0).
For the case where there are a lot of data modification operations, a higher
value could potentially have the data store consume more space and file handles.
For the case where there are a few data modification operations, a lower value
will impact performance due to no segment candidates available for
consolidation.

_Background:_
  For data modification ArangoSearch Views follow the concept of a
  "versioned data store". Thus old versions of data may be removed once there
  are no longer any users of the old data. The frequency of the cleanup and
  compaction operations are governed by 'consolidationIntervalMsec' and the
  candidates for compaction are selected via 'consolidationPolicy'.

@RESTBODYPARAM{consolidationPolicy,object,optional,post_api_index_inverted_policy}
The consolidation policy to apply for selecting which segments should be merged
(default: {}).

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
  released once old segments are no longer used.

@RESTSTRUCT{type,post_api_index_inverted_policy,string,optional,}
The segment candidates for the "consolidation" operation are selected based
upon several possible configurable formulas as defined by their types.
The supported types are:

- `"tier"` (default): consolidate based on segment byte size and live
  document count as dictated by the customization attributes.

@RESTSTRUCT{segmentsBytesFloor,post_api_index_inverted_policy,integer,optional,}
Defines the value (in bytes) to treat all smaller segments as equal for
consolidation selection. Default: `2097152`

@RESTSTRUCT{segmentsBytesMax,post_api_index_inverted_policy,integer,optional,}
The maximum allowed size of all consolidated segments in bytes.
Default: `5368709120`

@RESTSTRUCT{segmentsMax,post_api_index_inverted_policy,integer,optional,}
The maximum number of segments that are evaluated as candidates for
consolidation. Default: `10`

@RESTSTRUCT{segmentsMin,post_api_index_inverted_policy,integer,optional,}
The minimum number of segments that are evaluated as candidates for
consolidation. Default: `1`

@RESTSTRUCT{minScore,post_api_index_inverted_policy,integer,optional,}
Filter out consolidation candidates with a score less than this. Default: `0`

@RESTBODYPARAM{writebufferIdle,integer,optional,int64}
Maximum number of writers (segments) cached in the pool
(default: 64, use 0 to disable)

@RESTBODYPARAM{writebufferActive,integer,optional,int64}
Maximum number of concurrent active writers (segments) that perform a
transaction. Other writers (segments) wait till current active writers
(segments) finish (default: 0, use 0 to disable)

@RESTBODYPARAM{writebufferSizeMax,integer,optional,int64}
Maximum memory byte size per writer (segment) before a writer (segment) flush
is triggered. `0` value turns off this limit for any writer (buffer) and data
will be flushed periodically based on the value defined for the flush thread
(ArangoDB server startup option). `0` value should be used carefully due to
high potential memory consumption
(default: 33554432, use 0 to disable)

@RESTDESCRIPTION
Creates an inverted index for the collection `collection-name`, if
it does not already exist. The call expects an object containing the index
details.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index already exists, then a *HTTP 200* is returned.

@RESTRETURNCODE{201}
If the index does not already exist and can be created, then a *HTTP 201*
is returned.

@RESTRETURNCODE{404}
If the `collection-name` is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Creating an inverted index:

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewInverted}
    var cn = "products";
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "inverted",
      fields: [ "a", { name: "b", analyzer: "text_en" } ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
