@startDocuBlock post_api_view_arangosearch
@brief creates an ArangoSearch View

@RESTHEADER{POST /_api/view#arangosearch, Create an ArangoSearch View, createView}

@RESTBODYPARAM{name,string,required,string}
The name of the View.

@RESTBODYPARAM{type,string,required,string}
The type of the View (immutable). Must be equal to *"arangosearch"*.

@RESTBODYPARAM{cleanupIntervalStep,integer,optional,uint64}
Wait at least this many commits between removing unused files in the
ArangoSearch data directory (default: 10, to disable use: 0).
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

@RESTBODYPARAM{commitIntervalMsec,integer,optional,uint64}
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

@RESTBODYPARAM{consolidationIntervalMsec,integer,optional,uint64}
Wait at least this many milliseconds between applying 'consolidationPolicy' to
consolidate View data store and possibly release space on the filesystem
(default: 60000, to disable use: 0).
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

@RESTBODYPARAM{consolidationPolicy,object,optional,post_api_view_props_consolidation}
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
  released once old segments are no longer used.

@RESTSTRUCT{type,post_api_view_props_consolidation,string,optional,string}
The segment candidates for the "consolidation" operation are selected based
upon several possible configurable formulas as defined by their types.
The currently supported types are (default: "bytes_accum"):
- *bytes_accum*: consolidate if and only if (`{threshold}` range `[0.0, 1.0]`):
  `{threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes`
  i.e. the sum of all candidate segment byte size is less than the total
  segment byte size multiplied by the `{threshold}`
- *tier*: consolidate based on segment byte size and live document count
  as dictated by the customization attributes.

@RESTBODYPARAM{links,object,optional,post_api_view_links}
Expects an object with the attribute keys being names of to be linked collections,
and the link properties as attribute values.

@RESTSTRUCT{[collection-name],post_api_view_links,object,optional,post_api_view_link_props}
Name of a collection as attribute key.

@RESTSTRUCT{analyzers,post_api_view_link_props,array,optional,string}
The list of analyzers to be used for indexing of string values
(default: ["identity"]).

@RESTSTRUCT{fields,post_api_view_link_props,object,optional,post_api_view_fields}
The field properties. If specified, then *fields* should be a JSON object
containing the following attributes:

@RESTSTRUCT{[field-name],post_api_view_fields,object,optional,object}
This is a recursive structure for the specific attribute path, potentially
containing any of the following attributes:
*analyzers*, *fields*, *includeAllFields*, *trackListPositions*, *storeValues*
Any attributes not specified are inherited from the parent.

@RESTSTRUCT{includeAllFields,post_api_view_link_props,boolean,optional,bool}
The flag determines whether or not to index all fields on a particular level of
depth (default: false).

@RESTSTRUCT{trackListPositions,post_api_view_link_props,boolean,optional,bool}
The flag determines whether or not values in a lists should be treated separate
(default: false).

@RESTSTRUCT{storeValues,post_api_view_link_props,string,optional,string}
How should the View track the attribute values, this setting allows for
additional value retrieval optimizations, one of:
- *none*: Do not store values by the View
- *id*: Store only information about value presence, to allow use of the EXISTS() function
(default "none").

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
