HTTP Interface for Edges {#RestEdge}
====================================

@NAVIGATE_RestEdge
@EMBEDTOC{RestEdgeTOC}

This is an introduction to ArangoDB's REST interface for edges.

ArangoDB offers also some graph functionality. A graph consists of nodes, edges
and properties. ArangoDB stores the information how the nodes relate to each
other aside from the properties.

A graph data model always consists of two collections: the relations between the
nodes in the graphs are stored in an "edges collection", the nodes in the graph
are stored in documents in regular collections.

Example:
- the "edge" collection stores the information that a company's reception is
  sub-unit to the services unit and the services unit is sub-unit to the
  CEO. You would express this relationship with the `to` and `_to` attributes.
- the "normal" collection stores all the properties about the reception,
  e.g. that 20 people are working there and the room number etc.
- `_from` is the document handle of the linked vertex (incoming relation),
- `_to` is the document handle of the linked vertex (outgoing relation).

Documents, Identifiers, Handles {#RestEdgeIntro}
================================================

@copydoc GlossaryEdge

Address and ETag of an Edge {#RestEdgeResource}
===============================================

All documents in ArangoDB have a document handle. This handle uniquely identifies 
a document. Any document can be retrieved using its unique URI:

    http://server:port/_api/document/<document-handle>

Edges are a special variation of documents, and to work with edges, the above URL
format changes to:

    http://server:port/_api/edge/<document-handle>

For example, assumed that the document handle, which is stored in the `_id`
attribute of the edge, is `demo/362549736`, then the URL of that edge is:

    http://localhost:8529/_api/edge/demo/362549736

The above URL scheme does not specify a database name explicitly, so the 
default database will be used. To explicitly specify the database context, use
the following URL schema:

    http://server:port/_db/<database-name>/_api/edge/<document-handle>

Example:

    http://localhost:8529/_db/mydb/_api/edge/demo/362549736

Note that the following examples use the short URL format for brevity.

Working with Edges using REST {#RestEdgeHttp}
=============================================

@anchor RestEdgeRead
@RESTHEADER{GET /_api/edge,reads an edge}

@REST{GET /_api/edge/@FA{document-handle}}

See @ref RestDocument for details.

@anchor RestEdgeCreate
@copydetails triagens::arango::RestEdgeHandler::createDocument

@anchor RestEdgeUpdate
@RESTHEADER{PUT /_api/edge,updates an edge}

@REST{PUT /_api/edge/@FA{document-handle}}

See @ref RestDocument for details.

@anchor RestEdgePatch
@RESTHEADER{PATCH /_api/edge,partially updates an edge}

@REST{PATCH /_api/edge/@FA{document-handle}}

See @ref RestDocument for details.

@anchor RestEdgeDelete
@RESTHEADER{DELETE /_api/edge,deletes an edge}

@REST{DELETE /_api/edge/@FA{document-handle}}

See @ref RestDocument for details.

@anchor RestEdgeHead
@RESTHEADER{GET /_api/edge,reads an edge header}

@REST{HEAD /_api/edge/@FA{document-handle}}

See @ref RestDocument for details.

@anchor RestEdgeEdges
@copydetails JSF_get_edges
