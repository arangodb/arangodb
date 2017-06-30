Edges, Identifiers, Handles
===========================

This is an introduction to ArangoDB's interface for edges.
Edges may be [used in graphs](../README.md).
Here we work with edges from the JavaScript shell *arangosh*.
For other languages see the corresponding language API.

A graph data model always consists of at least two collections: the relations between the
nodes in the graphs are stored in an "edges collection", the nodes in the graph
are stored in documents in regular collections.

Edges in ArangoDB are special documents. In addition to the system
attributes *_key*, *_id* and *_rev*, they have the attributes *_from* and *_to*, 
which contain [document handles](../../Appendix/Glossary.md#document-handle), namely the start-point and the end-point of the edge.

*Example*:

- the "edge" collection stores the information that a company's reception is sub-unit to the services unit and the services unit is sub-unit to the
  CEO. You would express this relationship with the *_from* and *_to* attributes
- the "normal" collection stores all the properties about the reception, e.g. that 20 people are working there and the room number etc
- *_from* is the [document handle](../../Appendix/Glossary.md#document-handle) of the linked vertex (incoming relation)
- *_to* is the document handle of the linked vertex (outgoing relation)

[Edge collections](../../Appendix/Glossary.md#edge-collection) are special collections that store edge documents. Edge documents 
are connection documents that reference other documents. The type of a collection 
must be specified when a collection is created and cannot be changed afterwards.

To change edge endpoints you would need to remove old document/edge and insert new one.
Other fields can be updated as in default collection.

Working with Edges
------------------

Edges are normal [documents](../../DataModeling/Documents/DocumentMethods.md#edges)
that always contain a `_from` and a `_to` attribute.
