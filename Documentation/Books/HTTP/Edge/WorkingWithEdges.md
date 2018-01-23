Working with Edges using the RESTful API
========================================

This is documentation to ArangoDB's
[RESTful interface for edges](../../Manual/Graphs/Edges/index.html).

Edges are documents with two additional attributes: *_from* and *_to*.
These attributes are mandatory and must contain the document-handle
of the from and to vertices of an edge.

Use the general document
[RESTful api](../Document/WorkingWithDocuments.md)
for create/read/update/delete.

<!-- Rest/Graph edges -->
@startDocuBlock API_EDGE_READINOUTBOUND
