@startDocuBlock patch_api_view_properties_iresearch
@brief partially changes properties of an ArangoSearch view

@RESTHEADER{PATCH /_api/view/{view-name}/properties#ArangoSearch, Partially changes properties of an ArangoSearch view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTBODYPARAM{properties,object,optional,post_api_view_props}
The view properties. If specified, then *properties* should be a JSON object
containing the following attributes:

@RESTSTRUCT{cleanupIntervalStep,post_api_view_props,integer,optional,uint64}
Wait at least this many commits between removing unused files in the
ArangoSearch data directory (default: 10, to disable use: 0).
For the case where the consolidation policies merge segments often (i.e. a lot
of commit+consolidate), a lower value will cause a lot of disk space to be
wasted.
For the case where the consolidation policies rarely merge segments (i.e. few
inserts/deletes), a higher value will impact performance without any added
benefits.
Background:
  With every "commit" or "consolidate" operation a new state of the view
  internal data-structures is created on disk.
  Old states/snapshots are released once there are no longer any users
  remaining.
  However, the files for the released states/snapshots are left on disk, and
  only removed by "cleanup" operation.

@RESTSTRUCT{consolidationIntervalMsec,post_api_view_props,integer,optional,uint64}
Wait at least this many milliseconds between committing view data store
changes and making documents visible to queries (default: 60000, to disable
use: 0).
For the case where there are a lot of inserts/updates, a lower value, until
commit, will cause the index not to account for them and memory usage would
continue to grow.
For the case where there are a few inserts/updates, a higher value will impact
performance and waste disk space for each commit call without any added
benefits.
Background:
  For data retrieval ArangoSearch views follow the concept of
  "eventually-consistent", i.e. eventually all the data in ArangoDB will be
  matched by corresponding query expressions.
  The concept of ArangoSearch view "commit" operation is introduced to
  control the upper-bound on the time until document addition/removals are
  actually reflected by corresponding query expressions.
  Once a "commit" operation is complete all documents added/removed prior to
  the start of the "commit" operation will be reflected by queries invoked in
  subsequent ArangoDB transactions, in-progress ArangoDB transactions will
  still continue to return a repeatable-read state.


@RESTSTRUCT{consolidationPolicy,post_api_view_props,object,optional,post_api_view_props_consolidation}
The consolidation policy to apply for selecting which segments should be merged
(default: {})
Background:
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


@RESTSTRUCT{type,post_api_view_props_consolidations,string,optional,string}
The segment candidates for the "consolidation" operation are selected based
upon several possible configurable formulas as defined by their types.
The currently supported types are (default: "bytes_accum"):
- *bytes_accum*: consolidate if and only if ({threshold} range `[0.0, 1.0]`):
  {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
  i.e. the sum of all candidate segment byte size is less than the total
       segment byte size multiplied by the {threshold}
- *tier*: consolidate based on segment byte size and live document count
          as dicated by the customization attributes.


@RESTSTRUCT{links,post_api_view_props,object,optional,post_api_view_links}
The set of collection names associated with the properties.


@RESTSTRUCT{[collection-name],post_api_view_links,object,optional,post_api_view_link_props}
The link properties. If specified, then *properties* should be a JSON object
containing the following attributes:

@RESTSTRUCT{analyzers,post_api_view_link_props,array,optional,string}
The list of analyzers to be used for indexing of string values
(default: ["identity"]).


@RESTSTRUCT{fields,post_api_view_link_props,object,optional,post_api_view_fields}
The field properties. If specified, then *properties* should be a JSON object
containing the following attributes:

@RESTSTRUCT{field-name,post_api_view_fields,array,optional,object}
This is a recursive structure for the specific attribute path, potentially
containing any of the following attributes:
*analyzers*, *includeAllFields*, *trackListPositions*, *storeValues*
Any attributes not specified are inherited from the parent.


@RESTSTRUCT{includeAllFields,post_api_view_link_props,boolean,optional,bool}
The flag determines whether or not to index all fields on a particular level of
depth (default: false).

@RESTSTRUCT{trackListPositions,post_api_view_link_props,boolean,optional,bool}
The flag determines whether or not values in a lists should be treated separate
(default: false).

@RESTSTRUCT{storeValues,post_api_view_link_props,string,optional,string}
How should the view track the attribute values, this setting allows for
additional value retrieval optimizations, one of:
- *none*: Do not store values by the view
- *id*: Store only information about value presence, to allow use of the EXISTS() function
(default "none").


@RESTDESCRIPTION
Changes the properties of a view.

On success an object with the following attributes is returned:
- *id*: The identifier of the view
- *name*: The name of the view
- *type*: The view type
- all additional arangosearch view implementation specific properties

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestIResearchViewPatchProperties}
    var viewName = "products";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/"+ view.name() + "/properties";

    var response = logCurlRequest('PATCH', url, { "locale": "en" });

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
