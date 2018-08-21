@startDocuBlock post_api_view_iresearch
@brief creates an ArangoSearch view

@RESTHEADER{POST /_api/view#ArangoSearch, Create an ArangoSearch view}

@RESTBODYPARAM{name,string,required,string}
The name of the view.

@RESTBODYPARAM{type,string,required,string}
The type of the view. must be equal to *"arangosearch"*

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


@RESTSTRUCT{type,post_api_view_props_consolidations,string,optional,string}
Currently supported types aren (default: "bytes_accum"):
- *bytes*: Consolidate IFF {threshold} > segment_bytes / (all_segment_bytes / #segments)
- *bytes_accum*: Consolidate IFF {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
- *count*: Consolidate IFF {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
- *fill*: Consolidate IFF {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})

@RESTSTRUCT{segmentThreshold,post_api_view_props_consolidation,integer,optional,uint64}
Apply consolidation policy IFF {segmentThreshold} > #segments (default: 300)

@RESTSTRUCT{threshold,post_api_view_props_consolidation,number,optional,float}
Consolidate IFF {threshold} > {formula based on *type*}, valid value range [0.0, 1.0] (default: 0.85)


@RESTSTRUCT{locale,post_api_view_props,string,optional,string}
The default locale used for queries on analyzed string values (default: *C*).


@RESTDESCRIPTION
Creates a new view with a given name and properties if it does not
already exist.

**Note**: view can't be created with the links. Please use PUT/PATCH for links
management.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPostView}
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
