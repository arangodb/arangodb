HTTP Interface for AQL Queries {#HttpQuery}
===========================================

@NAVIGATE_HttpQuery
@EMBEDTOC{HttpQueryTOC}

ArangoDB has an Http interface to syntactically validate AQL queries.
Furthermore, it offers an Http interface to retrieve the execution plan for any
valid AQL query.

Both functionalities do not actually execute the supplied AQL query, but only
inspect it and return meta information about it.

@anchor HttpExplainPost
@copydetails JSF_post_api_explain

@CLEARPAGE
@anchor HttpQueryPost
@copydetails JSF_post_api_query
