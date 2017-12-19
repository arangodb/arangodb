!CHAPTER How to invoke AQL 

AQL queries can be executed using:

- the web interface,
- the `db` object (either in arangosh or in a Foxx service)
- or the raw HTTP API.

There are always calls to the server's API under the hood, but the web interface
and the `db` object abstract away the low-level communication details and are
thus easier to use.

The ArangoDB Web Interface has a [specific tab for AQL queries execution](../Invocation/WithWebInterface.md).

You can run [AQL queries from the ArangoDB Shell](../Invocation/WithArangosh.md)
with the [_query](WithArangosh.html#with-dbquery) and
[_createStatement](WithArangosh.html#with-createstatement-arangostatement) methods
of the [`db` object](../../Manual/Appendix/References/DBObject.html). This chapter
also describes how to use bind parameters, statistics, counting and cursors with
arangosh.

If you are using Foxx, see [how to write database queries](../../Manual/Foxx/GettingStarted.html#writing-database-queries)
for examples including tagged template strings.

If you want to run AQL queries from your application via the HTTP REST API,
see the full API description at [HTTP Interface for AQL Query Cursors](../../HTTP/AqlQueryCursor/index.html).
