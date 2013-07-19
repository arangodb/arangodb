HTTP Interface for Simple Queries {#HttpSimple}
===============================================

@NAVIGATE_HttpSimple
@EMBEDTOC{HttpSimpleTOC}

Simple Queries {#HttpSimpleIntro}
=================================

This is an introduction to ArangoDB's Http interface for simple queries.

Simple queries can be used if the query condition is straight forward simple,
i.e., a document reference, all documents, a query-by-example, or a simple geo
query. In a simple query you can specify exactly one collection and one
condition. The result can then be sorted and can be split into pages.

Working with Simples Queries using HTTP {#HttpSimpleHttp}
=========================================================

To limit the amount of results to be transferred in one batch, simple queries
support a `batchSize` parameter that can optionally be used to tell the server
to limit the number of results to be transferred in one batch to a certain
value. If the query has more results than were transferred in one go, more
results are waiting on the server so they can be fetched subsequently. If no
value for the `batchSize` parameter is specified, the server will use a
reasonable default value.

If the server has more documents than should be returned in a single batch, the
server will set the `hasMore` attribute in the result. It will also return the
id of the server-side cursor in the `id` attribute in the result.  This id can
be used with the cursor API to fetch any outstanding results from the server and
dispose the server-side cursor afterwards.

@anchor HttpSimpleAll
@copydetails JSA_put_api_simple_all

@CLEARPAGE
@anchor HttpSimpleByExample
@copydetails JSA_put_api_simple_by_example

@CLEARPAGE
@anchor HttpSimpleFirstExample
@copydetails JSA_put_api_simple_first_example

@CLEARPAGE
@anchor HttpSimpleAny
@copydetails JSA_put_api_simple_any

@CLEARPAGE
@anchor HttpSimpleRange
@copydetails JSA_put_api_simple_range

@CLEARPAGE
@anchor HttpSimpleNear
@copydetails JSA_put_api_simple_near

@CLEARPAGE
@anchor HttpSimpleWithin
@copydetails JSA_put_api_simple_within

@CLEARPAGE
@anchor HttpSimpleFulltext
@copydetails JSA_put_api_simple_fulltext

@CLEARPAGE
@anchor HttpSimpleRemoveByExample
@copydetails JSA_put_api_simple_remove_by_example

@CLEARPAGE
@anchor HttpSimpleReplaceByExample
@copydetails JSA_put_api_simple_replace_by_example

@CLEARPAGE
@anchor HttpSimpleUpdateByExample
@copydetails JSA_put_api_simple_update_by_example

@CLEARPAGE
@anchor HttpSimpleFirst
@copydetails JSA_put_api_simple_first

@CLEARPAGE
@anchor HttpSimpleLast
@copydetails JSA_put_api_simple_last
