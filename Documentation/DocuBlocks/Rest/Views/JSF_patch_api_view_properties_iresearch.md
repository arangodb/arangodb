
@startDocuBlock JSF_patch_api_view_properties_iresearch
@brief partially changes properties of an iresearch view

@RESTHEADER{PATCH /_api/view/{view-name}/properties#iresearch, Partially changes properties of an iresearch view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTBODYPARAM{links,object,optional,JSF_patch_api_view_links}
The set of collection names associated with the properties.

@RESTSTRUCT{collection-name,JSF_patch_api_view_links,object,optional,JSF_patch_api_view_link_props}
The link properties. If specified, then *properties*
should be a JSON object containing the following attributes:

@RESTSTRUCT{boost,JSF_patch_api_view_link_props,float,optional,float}
Multiplier of the rank value computed for documents matching on this field (default: 1.0).

@RESTSTRUCT{includeAllFields,JSF_patch_api_view_link_props,boolean,optional,bool}
The flag determines whether or not to index all fields on a particular level of depth (default: false).

@RESTSTRUCT{trackListPositions,JSF_patch_api_view_link_props,boolean,optional,bool}
The flag determines whether or not values in a lists should be treated separate (default: false).

@RESTSTRUCT{analyzers,JSF_patch_api_view_link_props,array,optional,string}
The list of analyzers to be used for indexing of string values (default: ["identity"]).

@RESTSTRUCT{[field-name],JSF_patch_api_view_link_props,object,optional,JSF_patch_api_view_link_props_field_props}
The field properties. If specified, then *properties*
should be a JSON object containing the following attributes:

@RESTSTRUCT{boost,JSF_patch_api_view_link_props_field_props,float,optional,float}
Multiplier of the rank value computed for documents matching on this field (default: 1.0).

@RESTSTRUCT{includeAllFields,JSF_patch_api_view_link_props_field_props,boolean,optional,bool}
The flag determines whether or not to index all fields on a particular level of depth (default: false).

@RESTSTRUCT{trackListPositions,JSF_patch_api_view_link_props_field_props,boolean,optional,bool}
The flag determines whether or not values in a lists should be treated separate (default: false).

@RESTSTRUCT{analyzers,JSF_patch_api_view_link_props_field_props,array,optional,string}
The list of analyzers to be used for indexing of string values (default: ["identity"]).

@RESTSTRUCT{[field-name],JSF_patch_api_view_link_props_field_props,string,optional,}
Specify properties for nested fields here

@RESTBODYPARAM{commitBulk,object,optional,JSF_patch_api_view_props_commit_bulk}
Commit options for bulk operations (e.g. adding a new link).

@RESTSTRUCT{consolidate,JSF_patch_api_view_props_commit_bulk,object,optional,JSF_patch_api_view_props_consolidation}
A mapping of thresholds in the range [0, 1] to determine persistent store merge
candidate segments, if specified then only the listed policies are used, keys are any of (default: <none>):

@RESTSTRUCT{bytes,JSF_patch_api_view_props_consolidation,object,optional,JSF_patch_api_view_props_consolidation_bytes}
Use empty object for default values, i.e. {}

@RESTSTRUCT{intervalStep,JSF_patch_api_view_props_consolidation_bytes,integer,optional,uint64}
Apply consolidation policy with every Nth commit (default: 10, to disable use: 0)

@RESTSTRUCT{threshold,JSF_patch_api_view_props_consolidation_bytes,integer,optional,uint64}
Consolidate IFF {threshold} > segment_bytes / (all_segment_bytes / #segments) (default: 0.85)

@RESTSTRUCT{bytes_accum,JSF_patch_api_view_props_consolidation,object,optional,JSF_patch_api_view_props_consolidation_bytes_accum}
Use empty object for default values, i.e. {}

@RESTSTRUCT{intervalStep,JSF_patch_api_view_props_consolidation_bytes_accum,integer,optional,uint64}
Apply consolidation policy with every Nth commit (default: 10, to disable use: 0)

@RESTSTRUCT{threshold,JSF_patch_api_view_props_consolidation_bytes_accum,integer,optional,uint64}
Consolidate IFF {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes (default: 0.85)

@RESTSTRUCT{count,JSF_patch_api_view_props_consolidation,object,optional,JSF_patch_api_view_props_consolidation_count}
Use empty object for default values, i.e. {}

@RESTSTRUCT{intervalStep,JSF_patch_api_view_props_consolidation_count,integer,optional,uint64}
Apply consolidation policy with every Nth commit (default: 10, to disable use: 0)

@RESTSTRUCT{threshold,JSF_patch_api_view_props_consolidation_count,integer,optional,uint64}
Consolidate IFF {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments) (default: 0.85)

@RESTSTRUCT{fill,JSF_patch_api_view_props_consolidation,object,optional,JSF_patch_api_view_props_consolidation_fill}
Use empty object for default values, i.e. {}

@RESTSTRUCT{intervalStep,JSF_patch_api_view_props_consolidation_fill,integer,optional,uint64}
Apply consolidation policy with every Nth commit (default: 10, to disable use: 0)

@RESTSTRUCT{threshold,JSF_patch_api_view_props_consolidation_fill,integer,optional,uint64}
Consolidate IFF {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed}) (default: 0.85)

@RESTSTRUCT{commitIntervalBatchSize,JSF_patch_api_view_props_commit_bulk,integer,optional,uint64}
When bulk-indexing (i.e. adding a new link ) issue commit after <count>
records have been bulk indexed (default: 10000).
For the case where collection records contain a small number of indexable fields, a lower value would cause unnecessary
computation overhead and performance degradation.
For the case where collection records contain a large number of indexable fields, a higher value would cause higher memory
consumption between commits.

@RESTSTRUCT{cleanupIntervalStep,JSF_patch_api_view_props_commit_bulk,integer,optional,uint64}
Wait at least this many commits between removing unused files in data directory (default: 10,
to disable use: 0).
For the case where the consolidation policies merge segments often (i.e. a lot of commit+consolidate), a lower value will cause a
lot of disk space to be wasted.
For the case where the consolidation policies rarely merge segments (i.e. few inserts/deletes), a higher value will impact
performance without any added benefits.

@RESTBODYPARAM{commitItem,object,optional,JSF_patch_api_view_props_commit_item}
Commit options for regular operations.

@RESTSTRUCT{consolidate,JSF_patch_api_view_props_commit_item,object,optional,JSF_patch_api_view_props_consolidation}

@RESTSTRUCT{commitIntervalBatchSize,JSF_patch_api_view_props_commit_item,integer,optional,uint64}
Wait at least this many milliseconds between committing index data changes and
making them visible to queries (default: 60000, to disable use: 0).
For the case where there are a lot of inserts/updates, a lower value, until commit, will cause the index not to account for them and
memory usage would continue to grow.
For the case where there are a few inserts/updates, a higher value will impact performance and waste disk space for each
commit call without any added benefits.

@RESTBODYPARAM{locale,string,optional,string}
The default locale used for queries on analyzed string values (default: *C*).

@RESTBODYPARAM{dataPath,string,optional,string}
The filesystem path where to store persisted index data (default: *"<ArangoDB database path>/iresearch-<index id>"*).

@RESTBODYPARAM{threadMaxIdle,integer,optional,uint64}
Maximum idle number of threads for single-run tasks (default: 5).
For the case where there are a lot of short-lived asynchronous tasks, a lower value will cause a lot of thread creation/deletion calls.
For the case where there are no short-lived asynchronous tasks, a higher value will only waste memory.

@RESTBODYPARAM{threadMaxTotal,integer,optional,uint64}
Maximum total number of threads (>0) for single-run tasks (default: 5).
For the case where there are a lot of parallelizable tasks and an abundance of resources, a lower value would limit performance.
For the case where there are limited resources CPU/memory, a higher value will negatively impact performance.

@RESTDESCRIPTION
Changes the properties of a view.

On success an object with the following attributes is returned:
- *id*: The identifier of the view.
- *name*: The name of the view.
- *type*: The view type. Valid types are:
  - iresearch : IResearch view
- *properties*: The updated properties of the view.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestIResearchViewPatchProperties}
    var viewName = "products";
    var viewType = "iresearch";
    db._dropView(viewName);
    var view = db._createView(viewName, viewType, {});
    var url = "/_api/view/"+ view.name() + "/properties";

    var response = logCurlRequest('PATCH', url, { "threadMaxIdle" : 10 });

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

