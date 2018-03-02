Edges, Identifiers, Handles
===========================

This is an introduction to ArangoDB's interface for edges and how to handle
edges from the JavaScript shell *arangosh*. For other languages see the
corresponding language API.

Edges in ArangoDB are special documents. In addition to the internal 
attributes *_key*, *_id* and *_rev*, they have two attributes *_from* and *_to*, 
which contain [document handles](../Glossary/README.md#document-handle), namely the start-point and the end-point of the edge.
The values of *_from* and *_to* are immutable once saved.

[Edge collections](../Glossary/README.md#edge-collection) are special collections that store edge documents. Edge documents 
are connection documents that reference other documents. The type of a collection 
must be specified when a collection is created and cannot be changed afterwards.

To change edge endpoints you would need to remove old document/edge and insert new one.
Other fields can be updated as in default collection.

Working with Edges
------------------

### Insert
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock InsertEdgeCol

### Edges
<!-- arangod/V8Server/v8-query.cpp -->
@startDocuBlock edgeCollectionEdges

### InEdges
<!-- arangod/V8Server/v8-query.cpp -->
@startDocuBlock edgeCollectionInEdges

### OutEdges
<!-- arangod/V8Server/v8-query.cpp -->
@startDocuBlock edgeCollectionOutEdges
