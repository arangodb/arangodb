HTTP Interface for Simple Queries
=================================

{% hint 'warning' %}
The Simple Queries API is deprecated from version 3.4.0 on.
These endpoints should no longer be used.
They are superseded by AQL queries.
{% endhint %}

Simple Queries
--------------

This is an introduction to ArangoDB's HTTP interface for simple queries.

Simple queries can be used if the query condition is straight forward simple,
i.e., a document reference, all documents, a query-by-example, or a simple geo
query. In a simple query you can specify exactly one collection and one
condition. The result can then be sorted and can be split into pages.

Working with Simples Queries using HTTP
---------------------------------------

To limit the amount of results to be transferred in one batch, simple queries
support a *batchSize* parameter that can optionally be used to tell the server
to limit the number of results to be transferred in one batch to a certain
value. If the query has more results than were transferred in one go, more
results are waiting on the server so they can be fetched subsequently. If no
value for the *batchSize* parameter is specified, the server will use a
reasonable default value.

If the server has more documents than should be returned in a single batch, the
server will set the *hasMore* attribute in the result. It will also return the
id of the server-side cursor in the *id* attribute in the result.  This id can
be used with the cursor API to fetch any outstanding results from the server and
dispose the server-side cursor afterwards.

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_all

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_by_example

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_first_example

<!-- arangod/RestHandler/RestSimpleHandler.cpp -->
@startDocuBlock RestLookupByKeys

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_any

<!-- arangod/RestHandler/RestSimpleHandler.cpp -->
@startDocuBlock RestRemoveByKeys

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_remove_by_example

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_replace_by_example

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_update_by_example

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_range

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_near

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_within

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_within_rectangle

<!-- js/actions/api-simple.js -->
@startDocuBlock put_api_simple_fulltext
