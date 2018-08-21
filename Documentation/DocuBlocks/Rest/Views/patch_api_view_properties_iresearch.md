@startDocuBlock patch_api_view_properties_iresearch
@brief partially changes properties of an ArangoSearch view

@RESTHEADER{PATCH /_api/view/{view-name}/properties#ArangoSearch, Partially changes properties of an ArangoSearch view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTBODYPARAM{properties,object,optional,post_api_view_props}
The view properties. If specified, then *properties* should be a JSON object
containing the following attributes:

@RESTSTRUCT{commitIntervalMsec,post_api_view_props,integer,optional,uint64}
Wait at least this many milliseconds between committing index data changes and
making them visible to queries (default: 60000, to disable use: 0).
For the case where there are a lot of inserts/updates, a lower value, until
commit, will cause the index not to account for them and memory usage would
continue to grow.
For the case where there are a few inserts/updates, a higher value will impact
performance and waste disk space for each commit call without any added
benefits.

@RESTSTRUCT{cleanupIntervalStep,post_api_view_props,integer,optional,uint64}
Wait at least this many commits between removing unused files in data
directory (default: 10, to disable use: 0).
For the case where the consolidation policies merge segments often (i.e. a lot
of commit+consolidate), a lower value will cause a lot of disk space to be
wasted.
For the case where the consolidation policies rarely merge segments (i.e. few
inserts/deletes), a higher value will impact performance without any added
benefits.


@RESTSTRUCT{consolidate,post_api_view_props,object,optional,post_api_view_props_consolidation}
The consolidation policy to apply for selecting which segments should be merged
(default: {}, to disable use: null)


@RESTSTRUCT{type,optional,post_api_view_props_consolidations,string,optional,string}
Currently supported types are (default: "bytes_accum"):
- *bytes*: Consolidate IFF {threshold} > segment_bytes / (all_segment_bytes / #segments)
- *bytes_accum*: Consolidate IFF {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
- *count*: Consolidate IFF {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
- *fill*: Consolidate IFF {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})

@RESTSTRUCT{segmentThreshold,post_api_view_props_consolidation,integer,optional,uint64}
Apply consolidation policy IFF {segmentThreshold} > #segments (default: 300)

@RESTSTRUCT{threshold,post_api_view_props_consolidation,float,optional,float}
Consolidate IFF {threshold} > {formula based on *type*}, valid value range [0.0, 1.0] (default: 0.85)


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
