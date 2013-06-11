HTTP Interface for Traversals {#HttpTraversals}
===============================================

@NAVIGATE_HttpTraversals
@EMBEDTOC{HttpTraversalsTOC}

Traverals {#HttpTraversalsIntro}
================================

TODO: FIXME
ArangoDB's transactions are executed on the server. Transactions can be 
initiated by clients by sending the transaction description for execution to
the server.

Transactions in ArangoDB do not offer seperate `BEGIN`, `COMMIT` and `ROLLBACK`
operations as they are available in many other database products. 
Instead, ArangoDB transactions are described by a Javascript function, and the 
code inside the Javascript function will then be executed transactionally.
At the end of the function, the transaction is automatically committed, and all
changes done by the transaction will be persisted. If an exception is thrown
during transaction execution, all operations performed in the transaction are
rolled back.

For a more detailed description of how transactions work in ArangoDB please
refer to @ref Transactions. 

Executing Traversals via HTTP {#HttpTraversalsHttp}
=======================================================

@anchor HttpTraversalsPost
@copydetails JSF_post_api_traversal

