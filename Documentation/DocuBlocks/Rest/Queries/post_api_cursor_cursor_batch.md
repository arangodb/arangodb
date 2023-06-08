
@startDocuBlock post_api_cursor_cursor_batch

@RESTHEADER{POST /_api/cursor/{cursor-identifier}/{batch-identifier}, Read a batch from the cursor again, getPreviousAqlQueryCursorBatch}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The ID of the cursor.

@RESTURLPARAM{batch-identifier,string,required}
The ID of the batch. The first batch has an ID of `1` and the value is
incremented by 1 with every batch. You can only request the latest batch again
(or the next batch). Earlier batches are not kept on the server-side.

@RESTDESCRIPTION
You can use this endpoint to retry fetching the latest batch from a cursor.
The endpoint requires the `allowRetry` query option to be enabled for the cursor.

Calling this endpoint with the last returned batch identifier returns the
query results for that same batch again. This does not advance the cursor.
Client applications can use this to re-transfer a batch once more in case of
transfer errors.

You can also call this endpoint with the next batch identifier, i.e. the value
returned in the `nextBatchId` attribute of a previous request. This advances the
cursor and returns the results of the next batch.

From v3.11.1 onward, you may use this endpoint even if the `allowRetry`
attribute is `false` to fetch the next batch, but you cannot request a batch
again unless you set it to `true`.

Note that it is only supported to query the last returned batch identifier or
the directly following batch identifier. The latter is only supported if there
are more results in the cursor (i.e. `hasMore` is `true` in the latest batch).

Note that when the last batch has been consumed successfully by a client
application, it should explicitly delete the cursor to inform the server that it
successfully received and processed the batch so that the server can free up
resources.

The time-to-live for the cursor is renewed by this API call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server responds with *HTTP 200* in case of success.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (`false` in this case).

@RESTREPLYBODY{code,integer,required,int64}
The HTTP status code.

@RESTREPLYBODY{result,array,optional,}
An array of result documents for the current batch
(might be empty if the query has no results).

@RESTREPLYBODY{hasMore,boolean,required,}
A boolean indicator whether there are more results
available for the cursor on the server.

Note that even if `hasMore` returns `true`, the next call might still return no
documents. Once `hasMore` is `false`, the cursor is exhausted and the client
can stop asking for more results.

@RESTREPLYBODY{count,integer,optional,int64}
The total number of result documents available (only
available if the query was executed with the `count` attribute set).

@RESTREPLYBODY{id,string,optional,string}
The ID of the cursor for fetching more result batches.

@RESTREPLYBODY{nextBatchId,string,optional,string}
Only set if the `allowRetry` query option is enabled in v3.11.0.
From v3.11.1 onward, this attribute is always set, except in the last batch.

The ID of the batch after the current one. The first batch has an ID of `1` and
the value is incremented by 1 with every batch. You can remember and use this
batch ID should retrieving the next batch fail. Use the
`POST /_api/cursor/<cursor-id>/<batch-id>` endpoint to ask for the batch again.
You can also request the next batch.

@RESTREPLYBODY{extra,object,optional,post_api_cursor_extra}
An optional JSON object with extra information about the query result.

Only delivered as part of the first batch, or the last batch in case of a cursor
with the `stream` option enabled.

@RESTSTRUCT{warnings,post_api_cursor_extra,array,required,post_api_cursor_extra_warnings}
A list of query warnings.

@RESTSTRUCT{code,post_api_cursor_extra_warnings,integer,required,}
An error code.

@RESTSTRUCT{message,post_api_cursor_extra_warnings,string,required,}
A description of the problem.

@RESTSTRUCT{stats,post_api_cursor_extra,object,required,post_api_cursor_extra_stats}
An object with query statistics.

@RESTSTRUCT{writesExecuted,post_api_cursor_extra_stats,integer,required,}
The total number of data-modification operations successfully executed.

@RESTSTRUCT{writesIgnored,post_api_cursor_extra_stats,integer,required,}
The total number of data-modification operations that were unsuccessful,
but have been ignored because of the `ignoreErrors` query option.

@RESTSTRUCT{scannedFull,post_api_cursor_extra_stats,integer,required,}
The total number of documents iterated over when scanning a collection 
without an index. Documents scanned by subqueries are included in the result, but
operations triggered by built-in or user-defined AQL functions are not.

@RESTSTRUCT{scannedIndex,post_api_cursor_extra_stats,integer,required,}
The total number of documents iterated over when scanning a collection using
an index. Documents scanned by subqueries are included in the result, but operations
triggered by built-in or user-defined AQL functions are not.

@RESTSTRUCT{cursorsCreated,post_api_cursor_extra_stats,integer,required,}
The total number of cursor objects created during query execution. Cursor
objects are created for index lookups.

@RESTSTRUCT{cursorsRearmed,post_api_cursor_extra_stats,integer,required,}
The total number of times an existing cursor object was repurposed.
Repurposing an existing cursor object is normally more efficient compared to destroying an
existing cursor object and creating a new one from scratch.

@RESTSTRUCT{cacheHits,post_api_cursor_extra_stats,integer,required,}
The total number of index entries read from in-memory caches for indexes
of type edge or persistent. This value is only non-zero when reading from indexes
that have an in-memory cache enabled, and when the query allows using the in-memory
cache (i.e. using equality lookups on all index attributes).

@RESTSTRUCT{cacheMisses,post_api_cursor_extra_stats,integer,required,}
The total number of cache read attempts for index entries that could not
be served from in-memory caches for indexes of type edge or persistent. This value
is only non-zero when reading from indexes that have an in-memory cache enabled, the
query allows using the in-memory cache (i.e. using equality lookups on all index attributes)
and the looked up values are not present in the cache.

@RESTSTRUCT{filtered,post_api_cursor_extra_stats,integer,required,}
The total number of documents removed after executing a filter condition
in a `FilterNode` or another node that post-filters data. Note that nodes of the
`IndexNode` type can also filter documents by selecting only the required index range 
from a collection, and the `filtered` value only indicates how much filtering was done by a
post filter in the `IndexNode` itself or following `FilterNode` nodes.
Nodes of the `EnumerateCollectionNode` and `TraversalNode` types can also apply
filter conditions and can report the number of filtered documents.

@RESTSTRUCT{httpRequests,post_api_cursor_extra_stats,integer,required,}
The total number of cluster-internal HTTP requests performed.

@RESTSTRUCT{fullCount,post_api_cursor_extra_stats,integer,optional,}
The total number of documents that matched the search condition if the query's
final top-level `LIMIT` operation were not present.
This attribute may only be returned if the `fullCount` option was set when starting the 
query and only contains a sensible value if the query contains a `LIMIT` operation on
the top level.

@RESTSTRUCT{executionTime,post_api_cursor_extra_stats,number,required,}
The query execution time (wall-clock time) in seconds.

@RESTSTRUCT{peakMemoryUsage,post_api_cursor_extra_stats,integer,required,}
The maximum memory usage of the query while it was running. In a cluster,
the memory accounting is done per shard, and the memory usage reported is the peak
memory usage value from the individual shards.
Note that to keep things lightweight, the per-query memory usage is tracked on a relatively 
high level, not including any memory allocator overhead nor any memory used for temporary
results calculations (e.g. memory allocated/deallocated inside AQL expressions and function 
calls).

@RESTSTRUCT{intermediateCommits,post_api_cursor_extra_stats,integer,optional,}
The number of intermediate commits performed by the query. This is only non-zero
for write queries, and only for queries that reached either the `intermediateCommitSize`
or `intermediateCommitCount` thresholds. Note: in a cluster, intermediate commits can happen
on each participating DB-Server.

@RESTSTRUCT{nodes,post_api_cursor_extra_stats,array,optional,post_api_cursor_extra_stats_nodes}
When the query is executed with the `profile` option set to at least `2`,
then this attribute contains runtime statistics per query execution node.
For a human readable output, you can execute
`db._profileQuery(<query>, <bind-vars>)` in arangosh.

@RESTSTRUCT{id,post_api_cursor_extra_stats_nodes,integer,required,}
The execution node ID to correlate the statistics with the `plan` returned in
the `extra` attribute.

@RESTSTRUCT{calls,post_api_cursor_extra_stats_nodes,integer,required,}
The number of calls to this node.

@RESTSTRUCT{items,post_api_cursor_extra_stats_nodes,integer,required,}
The number of items returned by this node. Items are the temporary results
returned at this stage.

@RESTSTRUCT{runtime,post_api_cursor_extra_stats_nodes,number,required,}
The execution time of this node in seconds.

@RESTSTRUCT{profile,post_api_cursor_extra,object,optional,post_api_cursor_extra_profile}
The duration of the different query execution phases in seconds.

@RESTSTRUCT{initializing,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{parsing,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{optimizing ast,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{loading collections,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{instantiating plan,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{optimizing plan,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{instantiating executors,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{executing,post_api_cursor_extra_profile,number,required,}
@RESTSTRUCT{finalizing,post_api_cursor_extra_profile,number,required,}

@RESTSTRUCT{plan,post_api_cursor_extra,object,optional,post_api_cursor_extra_plan}
The execution plan.

@RESTSTRUCT{nodes,post_api_cursor_extra_plan,array,required,object}
A nested list of the execution plan nodes.

@RESTSTRUCT{rules,post_api_cursor_extra_plan,array,required,string}
A list with the names of the applied optimizer rules.

@RESTSTRUCT{collections,post_api_cursor_extra_plan,array,required,post_api_cursor_extra_plan_collections}
A list of the collections involved in the query. The list only includes the
collections that can statically be determined at query compile time.

@RESTSTRUCT{name,post_api_cursor_extra_plan_collections,string,required,}
The collection name.

@RESTSTRUCT{type,post_api_cursor_extra_plan_collections,string,required,}
How the collection is used. Can be `"read"`, `"write"`, or `"exclusive"`.

@RESTSTRUCT{variables,post_api_cursor_extra_plan,array,required,object}
All of the query variables, including user-created and internal ones.

@RESTSTRUCT{estimatedCost,post_api_cursor_extra_plan,integer,required,}
The estimated cost of the query.

@RESTSTRUCT{estimatedNrItems,post_api_cursor_extra_plan,integer,required,}
The estimated number of results.

@RESTSTRUCT{isModificationQuery,post_api_cursor_extra_plan,boolean,required,}
Whether the query contains write operations.

@RESTREPLYBODY{cached,boolean,required,}
A boolean flag indicating whether the query result was served
from the query cache or not. If the query result is served from the query
cache, the `extra` return attribute will not contain any `stats` sub-attribute
and no `profile` sub-attribute.

@RESTRETURNCODE{400}
If the cursor and the batch identifier are omitted, the server responds with
*HTTP 400*.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (`false` in this case).

@RESTREPLYBODY{code,integer,required,int64}
The HTTP status code.

@RESTREPLYBODY{errorNum,integer,required,int64}
A server error number (if `error` is `true`).

@RESTREPLYBODY{errorMessage,string,required,string}
A descriptive error message (if `error` is `true`).

@RESTRETURNCODE{404}
If no cursor with the specified identifier can be found, or if the requested
batch isn't available, the server responds with *HTTP 404*.

@RESTRETURNCODE{410}
The server responds with *HTTP 410* if a server which processes the query
or is the leader for a shard which is used in the query stops responding, but 
the connection has not been closed.

@RESTRETURNCODE{503}
The server responds with *HTTP 503* if a server which processes the query
or is the leader for a shard which is used in the query is down, either for 
going through a restart, a failure or connectivity issues.

@EXAMPLES

Request the second batch (again):

@EXAMPLE_ARANGOSH_RUN{RestCursorPostBatch}
    var url = "/_api/cursor";
    var body = {
      query: "FOR i IN 1..5 RETURN i",
      count: true,
      batchSize: 2,
      options: {
        allowRetry: true
      }
    };
    var response = logCurlRequest('POST', url, body);
    var secondBatchId = response.parsedBody.nextBatchId;
    assert(response.code === 201);
    logJsonResponse(response);

    response = logCurlRequest('POST', url + '/' + response.parsedBody.id, '');
    assert(response.code === 200);
    logJsonResponse(response);

    response = logCurlRequest('POST', url + '/' + response.parsedBody.id + '/' + secondBatchId, '');
    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

