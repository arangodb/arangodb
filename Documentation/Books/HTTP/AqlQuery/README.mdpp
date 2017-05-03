HTTP Interface for AQL Queries
==============================

### Explaining and parsing queries

ArangoDB has an HTTP interface to syntactically validate AQL queries.
Furthermore, it offers an HTTP interface to retrieve the execution plan for any
valid AQL query.

Both functionalities do not actually execute the supplied AQL query, but only
inspect it and return meta information about it.


<!-- js/actions/api-explain.js -->
@startDocuBlock JSF_post_api_explain

@startDocuBlock PostApiQueryProperties

### Query tracking

ArangoDB has an HTTP interface for retrieving the lists of currently
executing AQL queries and the list of slow AQL queries. In order to make meaningful
use of these APIs, query tracking needs to be enabled in the database the HTTP 
request is executed for.

<!--arangod/RestHandler/RestQueryHandler.cpp -->
@startDocuBlock GetApiQueryProperties

<!--arangod/RestHandler/RestQueryHandler.cpp -->
@startDocuBlock PutApiQueryProperties

<!--arangod/RestHandler/RestQueryHandler.cpp -->
@startDocuBlock GetApiQueryCurrent

<!--arangod/RestHandler/RestQueryHandler.cpp -->
@startDocuBlock GetApiQuerySlow

<!--arangod/RestHandler/RestQueryHandler.cpp -->
@startDocuBlock DeleteApiQuerySlow

### Killing queries

Running AQL queries can also be killed on the server. ArangoDB provides a kill facility
via an HTTP interface. To kill a running query, its id (as returned for the query in the
list of currently running queries) must be specified. The kill flag of the query will
then be set, and the query will be aborted as soon as it reaches a cancelation point.

<!--arangod/RestHandler/RestQueryHandler.cpp -->
@startDocuBlock DeleteApiQueryKill
