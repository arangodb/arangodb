
@startDocuBlock post_api_cursor
@brief create a cursor and return the first results

@RESTHEADER{POST /_api/cursor, Create cursor, createAqlQueryCursor}

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{x-arango-allow-dirty-read,boolean,optional}
Set this header to `true` to allow the Coordinator to ask any shard replica for
the data, not only the shard leader. This may result in "dirty reads".

The header is ignored if this operation is part of a Stream Transaction
(`x-arango-trx-id` header). The header set when creating the transaction decides
about dirty reads for the entire transaction, not the individual read operations.

@RESTHEADERPARAM{x-arango-trx-id,string,optional}
To make this operation a part of a Stream Transaction, set this header to the
transaction ID returned by the `POST /_api/transaction/begin` call.

@RESTBODYPARAM{query,string,required,string}
contains the query string to be executed

@RESTBODYPARAM{count,boolean,optional,}
indicates whether the number of documents in the result set should be returned in
the "count" attribute of the result.
Calculating the "count" attribute might have a performance impact for some queries
in the future so this option is turned off by default, and "count"
is only returned when requested.

@RESTBODYPARAM{batchSize,integer,optional,int64}
maximum number of result documents to be transferred from
the server to the client in one roundtrip. If this attribute is
not set, a server-controlled default value will be used. A *batchSize* value of
*0* is disallowed.

@RESTBODYPARAM{ttl,integer,optional,int64}
The time-to-live for the cursor (in seconds). If the result set is small enough
(less than or equal to `batchSize`) then results are returned right away.
Otherwise they are stored in memory and will be accessible via the cursor with
respect to the `ttl`. The cursor will be removed on the server automatically
after the specified amount of time. This is useful to ensure garbage collection
of cursors that are not fully fetched by clients. If not set, a server-defined
value will be used (default: 30 seconds).
The time-to-live is renewed upon every access to the cursor.

@RESTBODYPARAM{cache,boolean,optional,}
flag to determine whether the AQL query results cache
shall be used. If set to *false*, then any query cache lookup will be skipped
for the query. If set to *true*, it will lead to the query cache being checked
for the query if the query cache mode is either *on* or *demand*.

@RESTBODYPARAM{memoryLimit,integer,optional,int64}
the maximum number of memory (measured in bytes) that the query is allowed to
use. If set, then the query will fail with error "resource limit exceeded" in
case it allocates too much memory. A value of *0* indicates that there is no
memory limit.

@RESTBODYPARAM{bindVars,object,optional,}
An object with key/value pairs representing the bind parameters.
For a bind variable `@var` in the query, specify the value using an attribute
with the name `var`. For a collection bind variable `@@coll`, use `@coll` as the
attribute name. For example: `"bindVars": { "var": 42, "@coll": "products" }`.

@RESTBODYPARAM{options,object,optional,post_api_cursor_opts}
key/value object with extra options for the query.

@RESTSTRUCT{fullCount,post_api_cursor_opts,boolean,optional,}
if set to *true* and the query contains a *LIMIT* clause, then the
result will have an *extra* attribute with the sub-attributes *stats*
and *fullCount*, `{ ... , "extra": { "stats": { "fullCount": 123 } } }`.
The *fullCount* attribute will contain the number of documents in the result before the
last top-level LIMIT in the query was applied. It can be used to count the number of
documents that match certain filter criteria, but only return a subset of them, in one go.
It is thus similar to MySQL's *SQL_CALC_FOUND_ROWS* hint. Note that setting the option
will disable a few LIMIT optimizations and may lead to more documents being processed,
and thus make queries run longer. Note that the *fullCount* attribute may only
be present in the result if the query has a top-level LIMIT clause and the LIMIT
clause is actually used in the query.

@RESTSTRUCT{fillBlockCache,post_api_cursor_opts,boolean,optional,}
if set to *true* or not specified, this will make the query store the data it 
reads via the RocksDB storage engine in the RocksDB block cache. This is usually 
the desired behavior. The option can be set to *false* for queries that are
known to either read a lot of data which would thrash the block cache, or for queries
that read data which are known to be outside of the hot set. By setting the option
to *false*, data read by the query will not make it into the RocksDB block cache if
not already in there, thus leaving more room for the actual hot set.

@RESTSTRUCT{maxNumberOfPlans,post_api_cursor_opts,integer,optional,int64}
Limits the maximum number of plans that are created by the AQL query optimizer.

@RESTSTRUCT{maxNodesPerCallstack,post_api_cursor_opts,integer,optional,int64}
The number of execution nodes in the query plan after that stack splitting is
performed to avoid a potential stack overflow. Defaults to the configured value
of the startup option `--query.max-nodes-per-callstack`.

This option is only useful for testing and debugging and normally does not need
any adjustment.

@RESTSTRUCT{maxWarningCount,post_api_cursor_opts,integer,optional,int64}
Limits the maximum number of warnings a query will return. The number of warnings
a query will return is limited to 10 by default, but that number can be increased
or decreased by setting this attribute.

@RESTSTRUCT{failOnWarning,post_api_cursor_opts,boolean,optional,}
When set to *true*, the query will throw an exception and abort instead of producing
a warning. This option should be used during development to catch potential issues
early. When the attribute is set to *false*, warnings will not be propagated to
exceptions and will be returned with the query result.
There is also a server configuration option `--query.fail-on-warning` for setting the
default value for *failOnWarning* so it does not need to be set on a per-query level.

@RESTSTRUCT{allowRetry,post_api_cursor_opts,boolean,optional,}
Set this option to `true` to make it possible to retry
fetching the latest batch from a cursor. The default is `false`.

If retrieving a result batch fails because of a connection issue, you can ask
for that batch again using the `POST /_api/cursor/<cursor-id>/<batch-id>`
endpoint. The first batch has an ID of `1` and the value is incremented by 1
with every batch. Every result response except the last one also includes a
`nextBatchId` attribute, indicating the ID of the batch after the current.
You can remember and use this batch ID should retrieving the next batch fail.

You can only request the latest batch again (or the next batch).
Earlier batches are not kept on the server-side.
Requesting a batch again does not advance the cursor.

You can also call this endpoint with the next batch identifier, i.e. the value
returned in the `nextBatchId` attribute of a previous request. This advances the
cursor and returns the results of the next batch. This is only supported if there
are more results in the cursor (i.e. `hasMore` is `true` in the latest batch).

From v3.11.1 onward, you may use the `POST /_api/cursor/<cursor-id>/<batch-id>`
endpoint even if the `allowRetry` attribute is `false` to fetch the next batch,
but you cannot request a batch again unless you set it to `true`.

To allow refetching of the very last batch of the query, the server cannot
automatically delete the cursor. After the first attempt of fetching the last
batch, the server would normally delete the cursor to free up resources. As you
might need to reattempt the fetch, it needs to keep the final batch when the
`allowRetry` option is enabled. Once you successfully received the last batch,
you should call the `DELETE /_api/cursor/<cursor-id>` endpoint so that the
server doesn't unnecessarily keep the batch until the cursor times out
(`ttl` query option).

@RESTSTRUCT{stream,post_api_cursor_opts,boolean,optional,}
Can be enabled to execute the query lazily. If set to *true*, then the query is
executed as long as necessary to produce up to `batchSize` results. These
results are returned immediately and the query is suspended until the client
asks for the next batch (if there are more results). Depending on the query
this can mean that the first results will be available much faster and that
less memory is needed because the server only needs to store a subset of
results at a time. Read-only queries can benefit the most, unless `SORT`
without index or `COLLECT` are involved that make it necessary to process all
documents before a partial result can be returned. It is advisable to only use
this option for queries without exclusive locks.

Remarks:
- The query will hold resources until it ends (such as RocksDB snapshots, which
  prevents compaction to some degree). Writes will be in memory until the query
  is committed.
- If existing documents are modified, then write locks are held on these
  documents and other queries trying to modify the same documents will fail
  because of this conflict.
- A streaming query may fail late because of a conflict or for other reasons
  after some batches were already returned successfully, possibly rendering the
  results up to that point meaningless.
- The query options `cache`, `count` and `fullCount` are not supported for
  streaming queries.
- Query statistics, profiling data and warnings are delivered as part of the
  last batch.

If the `stream` option is *false* (default), then the complete result of the
query is calculated before any of it is returned to the client. The server
stores the full result in memory (on the contacted Coordinator if in a cluster).
All other resources are freed immediately (locks, RocksDB snapshots). The query
will fail before it returns results in case of a conflict.

@RESTSTRUCT{spillOverThresholdMemoryUsage,post_api_cursor_opts,integer,optional,}
This option allows queries to store intermediate and final results temporarily
on disk if the amount of memory used (in bytes) exceeds the specified value.
This is used for decreasing the memory usage during the query execution.

This option only has an effect on queries that use the `SORT` operation but
without a `LIMIT`, and if you enable the spillover feature by setting a path
for the directory to store the temporary data in with the
`--temp.intermediate-results-path` startup option.

Default value: 128MB.

**Note**:
Spilling data from RAM onto disk is an experimental feature and is turned off 
by default. The query results are still built up entirely in RAM on Coordinators
and single servers for non-streaming queries. To avoid the buildup of
the entire query result in RAM, use a streaming query (see the `stream` option).

@RESTSTRUCT{spillOverThresholdNumRows,post_api_cursor_opts,integer,optional,}
This option allows queries to store intermediate and final results temporarily
on disk if the number of rows produced by the query exceeds the specified value.
This is used for decreasing the memory usage during the query execution. In a
query that iterates over a collection that contains documents, each row is a
document, and in a query that iterates over temporary values 
(i.e. `FOR i IN 1..100`), each row is one of such temporary values.

This option only has an effect on queries that use the `SORT` operation but
without a `LIMIT`, and if you enable the spillover feature by setting a path
for the directory to store the temporary data in with the
`--temp.intermediate-results-path` startup option.

Default value: `5000000` rows.

**Note**:
Spilling data from RAM onto disk is an experimental feature and is turned off 
by default. The query results are still built up entirely in RAM on Coordinators
and single servers for non-streaming queries. To avoid the buildup of
the entire query result in RAM, use a streaming query (see the `stream` option).

@RESTSTRUCT{optimizer,post_api_cursor_opts,object,optional,post_api_cursor_opts_optimizer}
Options related to the query optimizer.

@RESTSTRUCT{rules,post_api_cursor_opts_optimizer,array,optional,string}
A list of to-be-included or to-be-excluded optimizer rules can be put into this
attribute, telling the optimizer to include or exclude specific rules. To disable
a rule, prefix its name with a `-`, to enable a rule, prefix it with a `+`. There is
also a pseudo-rule `all`, which matches all optimizer rules. `-all` disables all rules.

@RESTSTRUCT{profile,post_api_cursor_opts,integer,optional,}
If set to `true` or `1`, then the additional query profiling information is returned
in the `profile` sub-attribute of the `extra` return attribute, unless the query result
is served from the query cache. If set to `2`, the query includes execution stats
per query plan node in `stats.nodes` sub-attribute of the `extra` return attribute.
Additionally, the query plan is returned in the `extra.plan` sub-attribute.

@RESTSTRUCT{satelliteSyncWait,post_api_cursor_opts,number,optional,double}
This *Enterprise Edition* parameter allows to configure how long a DB-Server has time
to bring the SatelliteCollections involved in the query into sync.
The default value is `60.0` seconds. When the maximal time is reached, the query
is stopped.

@RESTSTRUCT{maxRuntime,post_api_cursor_opts,number,optional,double}
The query has to be executed within the given runtime or it is killed.
The value is specified in seconds. The default value is `0.0` (no timeout).

@RESTSTRUCT{maxDNFConditionMembers,post_api_cursor_opts,integer,optional,int64}
A threshold for the maximum number of `OR` sub-nodes in the internal
representation of an AQL `FILTER` condition.

Yon can use this option to limit the computation time and memory usage when
converting complex AQL `FILTER` conditions into the internal DNF
(disjunctive normal form) format. `FILTER` conditions with a lot of logical
branches (`AND`, `OR`, `NOT`) can take a large amount of processing time and
memory. This query option limits the computation time and memory usage for
such conditions.

Once the threshold value is reached during the DNF conversion of a `FILTER`
condition, the conversion is aborted, and the query continues with a simplified
internal representation of the condition, which **cannot be used for index lookups**.

You can set the threshold globally instead of per query with the
`--query.max-dnf-condition-members` startup option.

@RESTSTRUCT{maxTransactionSize,post_api_cursor_opts,integer,optional,int64}
The transaction size limit in bytes.

@RESTSTRUCT{intermediateCommitSize,post_api_cursor_opts,integer,optional,int64}
The maximum total size of operations after which an intermediate commit is performed
automatically.

@RESTSTRUCT{intermediateCommitCount,post_api_cursor_opts,integer,optional,int64}
The maximum number of operations after which an intermediate commit is performed
automatically.

@RESTSTRUCT{skipInaccessibleCollections,post_api_cursor_opts,boolean,optional,}
Let AQL queries (especially graph traversals) treat collection to which a user
has no access rights for as if these collections are empty. Instead of returning a
forbidden access error, your queries execute normally. This is intended to help
with certain use-cases: A graph contains several collections and different users
execute AQL queries on that graph. You can naturally limit the accessible
results by changing the access rights of users on collections.

This feature is only available in the Enterprise Edition.

@RESTSTRUCT{allowDirtyReads,post_api_cursor_opts,boolean,optional,}
If you set this option to `true` and execute the query against a cluster
deployment, then the Coordinator is allowed to read from any shard replica and
not only from the leader.

You may observe data inconsistencies (dirty reads) when reading from followers,
namely obsolete revisions of documents because changes have not yet been
replicated to the follower, as well as changes to documents before they are
officially committed on the leader.

This feature is only available in the Enterprise Edition.

@RESTDESCRIPTION
The query details include the query string plus optional query options and
bind parameters. These values need to be passed in a JSON representation in
the body of the POST request.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the result set can be created by the server.

@RESTREPLYBODY{,object,required,post_api_cursor_result}

@RESTRETURNCODE{400}
is returned if the JSON representation is malformed, the query specification is
missing from the request, or if the query is invalid.

The body of the response contains a JSON object with additional error
details. The object has the following attributes:

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate that an error occurred (*true* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{errorNum,integer,required,int64}
the server error number

@RESTREPLYBODY{errorMessage,string,required,string}
A descriptive error message.

If the query specification is complete, the server will process the query. If an
error occurs during query processing, the server will respond with *HTTP 400*.
Again, the body of the response will contain details about the error.

@RESTRETURNCODE{404}
The server will respond with *HTTP 404* in case a non-existing collection is
accessed in the query.

@RESTRETURNCODE{405}
The server will respond with *HTTP 405* if an unsupported HTTP method is used.

@RESTRETURNCODE{410}
The server will respond with *HTTP 410* if a server which processes the query
or is the leader for a shard which is used in the query stops responding, but 
the connection has not been closed.

@RESTRETURNCODE{503}
The server will respond with *HTTP 503* if a server which processes the query
or is the leader for a shard which is used in the query is down, either for 
going through a restart, a failure or connectivity issues.

@EXAMPLES

Execute a query and extract the result in a single go

@EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorForLimitReturnSingle}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"hello1":"world1"});
    db.products.save({"hello2":"world1"});

    var url = "/_api/cursor";
    var body = {
      query: "FOR p IN products LIMIT 2 RETURN p",
      count: true,
      batchSize: 2
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Execute a query and extract a part of the result

@EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorForLimitReturn}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"hello1":"world1"});
    db.products.save({"hello2":"world1"});
    db.products.save({"hello3":"world1"});
    db.products.save({"hello4":"world1"});
    db.products.save({"hello5":"world1"});

    var url = "/_api/cursor";
    var body = {
      query: "FOR p IN products LIMIT 5 RETURN p",
      count: true,
      batchSize: 2
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using the query option "fullCount"

@EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorOption}
    var url = "/_api/cursor";
    var body = {
      query: "FOR i IN 1..1000 FILTER i > 500 LIMIT 10 RETURN i",
      count: true,
      options: {
        fullCount: true
      }
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Enabling and disabling optimizer rules

@EXAMPLE_ARANGOSH_RUN{RestCursorOptimizerRules}
    var url = "/_api/cursor";
    var body = {
      query: "FOR i IN 1..10 LET a = 1 LET b = 2 FILTER a + b == 3 RETURN i",
      count: true,
      options: {
        maxPlans: 1,
        optimizer: {
          rules: [ "-all", "+remove-unnecessary-filters" ]
        }
      }
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Execute instrumented query and return result together with
execution plan and profiling information

@EXAMPLE_ARANGOSH_RUN{RestCursorProfileQuery}
    var url = "/_api/cursor";
    var body = {
      query: "LET s = SLEEP(0.25) LET t = SLEEP(0.5) RETURN 1",
      count: true,
      options: {
        profile: 2
      }
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Execute a data-modification query and retrieve the number of
modified documents

@EXAMPLE_ARANGOSH_RUN{RestCursorDeleteQuery}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"hello1":"world1"});
    db.products.save({"hello2":"world1"});

    var url = "/_api/cursor";
    var body = {
      query: "FOR p IN products REMOVE p IN products"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);
    assert(response.parsedBody.extra.stats.writesExecuted === 2);
    assert(response.parsedBody.extra.stats.writesIgnored === 0);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Execute a data-modification query with option *ignoreErrors*

@EXAMPLE_ARANGOSH_RUN{RestCursorDeleteIgnore}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({ _key: "foo" });

    var url = "/_api/cursor";
    var body = {
      query: "REMOVE 'bar' IN products OPTIONS { ignoreErrors: true }"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);
    assert(response.parsedBody.extra.stats.writesExecuted === 0);
    assert(response.parsedBody.extra.stats.writesIgnored === 1);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

The following example appends a value to the array `arr` of the document
with key `test` in the collection `documents`. The normal update behavior of the
`UPDATE` operation is to replace the array attribute completely, but using the
`PUSH()` function allows you to append to the array:

@EXAMPLE_ARANGOSH_RUN{RestCursorModifyArray}
    var cn = "documents";
    db._drop(cn);
    db._create(cn);

    db.documents.save({ _key: "test", arr: [1, 2, 3] });

    var url = "/_api/cursor";
    var body = {
      query: "FOR doc IN documents FILTER doc._key == @myKey UPDATE doc._key WITH { arr: PUSH(doc.arr, @value) } IN documents RETURN NEW",
      bindVars: { myKey: "test", value: 42 }
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 201);
    logJsonResponse(response);

    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

To set a memory limit for the query, the `memoryLimit` option can be passed to
the server.

The memory limit specifies the maximum number of bytes that the query is
allowed to use. When a single AQL query reaches the specified limit value, 
the query is aborted with a *resource limit exceeded* exception. In a 
cluster, the memory accounting is done per server, so the limit value is 
effectively a memory limit per query per server.

If no memory limit is specified, then the server default value (controlled by
startup option `--query.memory-limit`) is used for restricting the maximum amount
of memory the query can use. A memory limit value of `0` means that the maximum
amount of memory for the query is not restricted.

@EXAMPLE_ARANGOSH_RUN{RestCursorMemoryLimit}
    var url = "/_api/cursor";
    var body = {
      query: "FOR i IN 1..100000 SORT i RETURN i",
      memoryLimit: 100000
    }
    var response = logCurlRequest('POST', url, body);
    assert(response.code === 500);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Bad query - Missing body

@EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorMissingBody}
    var url = "/_api/cursor";

    var response = logCurlRequest('POST', url, '');

    assert(response.code === 400);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Bad query - Unknown collection

@EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorUnknownCollection}
    var url = "/_api/cursor";
    var body = {
      query: "FOR u IN unknowncoll LIMIT 2 RETURN u",
      count: true,
      batchSize: 2
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Bad query - Execute a data-modification query that attempts to remove a non-existing
document

@EXAMPLE_ARANGOSH_RUN{RestCursorDeleteQueryFail}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({ _key: "bar" });

    var url = "/_api/cursor";
    var body = {
      query: "REMOVE 'foo' IN products"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 404);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
