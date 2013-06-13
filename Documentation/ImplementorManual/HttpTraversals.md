HTTP Interface for Traversals {#HttpTraversals}
===============================================

@NAVIGATE_HttpTraversals
@EMBEDTOC{HttpTraversalsTOC}

Traverals {#HttpTraversalsIntro}
================================

ArangoDB's graph traversals are executed on the server. Traversals can be 
initiated by clients by sending the traversal description for execution to
the server.

Traversals in ArangoDB are used to walk over a graph
stored in one edge collection. It can easily be described
which edges of the graph should be followed and which actions
should be performed on each visited vertex.
Furthermore the ordering of visiting the nodes can be
specified, for instance depth-first or breadth-first search
are offered.

Executing Traversals via HTTP {#HttpTraversalsHttp}
=======================================================

@anchor HttpTraversalsPost
@copydetails JSF_post_api_traversal

