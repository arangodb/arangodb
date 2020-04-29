
@startDocuBlock get_api_replication_dump
@brief returns the whole content of one collection

@RESTHEADER{GET /_api/replication/dump, Return data of a collection,handleCommandDump}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The name or id of the collection to dump.

@RESTQUERYPARAM{chunkSize,number,optional}
Approximate maximum size of the returned result.

@RESTQUERYPARAM{batchId,number,required}
The id of the snapshot to use

@RESTDESCRIPTION
Returns the data from the collection for the requested range.


The *chunkSize* query parameter can be used to control the size of the result.
It must be specified in bytes. The *chunkSize* value will only be honored
approximately. Otherwise a too low *chunkSize* value could cause the server
to not be able to put just one entry into the result and return it.
Therefore, the *chunkSize* value will only be consulted after an entry has
been written into the result. If the result size is then bigger than
*chunkSize*, the server will respond with as many entries as there are
in the response already. If the result size is still smaller than *chunkSize*,
the server will try to return more data if there's more data left to return.

If *chunkSize* is not specified, some server-side default value will be used.

The *Content-Type* of the result is *application/x-arango-dump*. This is an
easy-to-process format, with all entries going onto separate lines in the
response body.

Each line itself is a JSON object, with at least the following attributes:

- *tick*: the operation's tick attribute

- *key*: the key of the document/edge or the key used in the deletion operation

- *rev*: the revision id of the document/edge or the deletion operation

- *data*: the actual document/edge data for types 2300 and 2301. The full
  document/edge data will be returned even for updates.

- *type*: the type of entry. Possible values for *type* are:

  - 2300: document insertion/update

  - 2301: edge insertion/update

  - 2302: document/edge deletion

**Note**: there will be no distinction between inserts and updates when calling this method.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully and data was returned. The header
`x-arango-replication-lastincluded` is set to the tick of the last document returned.

@RESTRETURNCODE{204}
is returned if the request was executed successfully, but there was no content available.
The header `x-arango-replication-lastincluded` is `0` in this case.

@RESTRETURNCODE{404}
is returned when the collection could not be found.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

<!-- TODO: How to know the RocksDB batchId?
 EXAMPLES

Empty collection:

 EXAMPLE_ARANGOSH_RUN{RestReplicationDumpEmpty_mmfiles}
    db._drop("testCollection");
    var c = db._create("testCollection");
    var url = "/_api/replication/dump?collection=" + c.name();
    var response = logCurlRequest('GET', url);

    assert(response.code === 204);
    logRawResponse(response);

    c.drop();
 END_EXAMPLE_ARANGOSH_RUN

Non-empty collection *(One JSON document per line)*:

 EXAMPLE_ARANGOSH_RUN{RestReplicationDump_mmfiles}
    db._drop("testCollection");
    var c = db._create("testCollection");
    c.save({ "test" : true, "a" : "abc", "_key" : "abcdef" });
    c.save({ "b" : 1, "c" : false, "_key" : "123456" });
    c.update("123456", { "d" : "additional value" });
    c.save({ "_key": "foobar" });
    c.remove("foobar");
    c.remove("abcdef");

    var url = "/_api/replication/dump?collection=" + c.name();
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);
    logJsonLResponse(response);

    c.drop();
 END_EXAMPLE_ARANGOSH_RUN
-->
@endDocuBlock
