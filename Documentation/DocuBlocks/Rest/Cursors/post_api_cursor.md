
@startDocuBlock post_api_cursor
@brief create a cursor and return the first results

@RESTHEADER{POST /_api/cursor, Create cursor, createQueryCursor}

A JSON object describing the query and query parameters.

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

@RESTBODYPARAM{bindVars,array,optional,object}
key/value pairs representing the bind parameters.

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

@RESTSTRUCT{maxPlans,post_api_cursor_opts,integer,optional,int64}
Limits the maximum number of plans that are created by the AQL query optimizer.

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

@RESTSTRUCT{optimizer,post_api_cursor_opts,object,optional,post_api_cursor_opts_optimizer}
Options related to the query optimizer.

@RESTSTRUCT{rules,post_api_cursor_opts_optimizer,array,optional,string}
A list of to-be-included or to-be-excluded optimizer rules can be put into this
attribute, telling the optimizer to include or exclude specific rules. To disable
a rule, prefix its name with a `-`, to enable a rule, prefix it with a `+`. There is
also a pseudo-rule `all`, which matches all optimizer rules. `-all` disables all rules.

@RESTSTRUCT{profile,post_api_cursor_opts,integer,optional,}
If set to *true* or *1*, then the additional query profiling information will be returned
in the sub-attribute *profile* of the *extra* return attribute, if the query result
is not served from the query cache. Set to *2* the query will include execution stats
per query plan node in sub-attribute *stats.nodes* of the *extra* return attribute.
Additionally the query plan is returned in the sub-attribute *extra.plan*.

@RESTSTRUCT{satelliteSyncWait,post_api_cursor_opts,number,optional,double}
This *Enterprise Edition* parameter allows to configure how long a DB-Server will have time
to bring the SatelliteCollections involved in the query into sync.
The default value is *60.0* (seconds). When the max time has been reached the query
will be stopped.

@RESTSTRUCT{maxRuntime,post_api_cursor_opts,number,optional,double}
The query has to be executed within the given runtime or it will be killed.
The value is specified in seconds. The default value is *0.0* (no timeout).

@RESTSTRUCT{maxTransactionSize,post_api_cursor_opts,integer,optional,int64}
Transaction size limit in bytes. Honored by the RocksDB storage engine only.

@RESTSTRUCT{intermediateCommitSize,post_api_cursor_opts,integer,optional,int64}
Maximum total size of operations after which an intermediate commit is performed
automatically. Honored by the RocksDB storage engine only.

@RESTSTRUCT{intermediateCommitCount,post_api_cursor_opts,integer,optional,int64}
Maximum number of operations after which an intermediate commit is performed
automatically. Honored by the RocksDB storage engine only.

@RESTSTRUCT{skipInaccessibleCollections,post_api_cursor_opts,boolean,optional,}
AQL queries (especially graph traversals) will treat collection to which a user has no access rights as if these collections were empty. Instead of returning a forbidden access error, your queries will execute normally. This is intended to help with certain use-cases: A graph contains several collections and different users execute AQL queries on that graph. You can now naturally limit the accessible results by changing the access rights of users on collections. This feature is only available in the Enterprise Edition.

@RESTDESCRIPTION
The query details include the query string plus optional query options and
bind parameters. These values need to be passed in a JSON representation in
the body of the POST request.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the result set can be created by the server.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,integer}
the HTTP status code

@RESTREPLYBODY{result,array,optional,}
an array of result documents (might be empty if query has no results)

@RESTREPLYBODY{hasMore,boolean,required,}
A boolean indicator whether there are more results
available for the cursor on the server

@RESTREPLYBODY{count,integer,optional,int64}
the total number of result documents available (only
available if the query was executed with the *count* attribute set)

@RESTREPLYBODY{id,string,required,string}
id of temporary cursor created on the server (optional, see above)

@RESTREPLYBODY{extra,object,optional,}
an optional JSON object with extra information about the query result
contained in its *stats* sub-attribute. For data-modification queries, the
*extra.stats* sub-attribute will contain the number of modified documents and
the number of documents that could not be modified
due to an error (if *ignoreErrors* query option is specified)

@RESTREPLYBODY{cached,boolean,required,}
a boolean flag indicating whether the query result was served
from the query cache or not. If the query result is served from the query
cache, the *extra* return attribute will not contain any *stats* sub-attribute
and no *profile* sub-attribute.

@RESTRETURNCODE{400}
is returned if the JSON representation is malformed or the query specification is
missing from the request.

If the JSON representation is malformed or the query specification is
missing from the request, the server will respond with *HTTP 400*.

The body of the response will contain a JSON object with additional error
details. The object has the following attributes:

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate that an error occurred (*true* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{errorNum,integer,required,int64}
the server error number

@RESTREPLYBODY{errorMessage,string,required,string}
a descriptive error message<br>
If the query specification is complete, the server will process the query. If an
error occurs during query processing, the server will respond with *HTTP 400*.
Again, the body of the response will contain details about the error.

@RESTRETURNCODE{404}
The server will respond with *HTTP 404* in case a non-existing collection is
accessed in the query.

@RESTRETURNCODE{405}
The server will respond with *HTTP 405* if an unsupported HTTP method is used.

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
    assert(JSON.parse(response.body).extra.stats.writesExecuted === 2);
    assert(JSON.parse(response.body).extra.stats.writesIgnored === 0);

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
    assert(JSON.parse(response.body).extra.stats.writesExecuted === 0);
    assert(JSON.parse(response.body).extra.stats.writesIgnored === 1);

    logJsonResponse(response);
  ~ db._drop(cn);
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
