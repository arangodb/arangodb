---
layout: default
description: This is documentation to ArangoDB'sREST interface for edges
---
Working with Edges using REST
=============================

This is documentation to ArangoDB's
[REST interface for edges](../graphs-edges.html).

Edges are documents with two additional attributes: *_from* and *_to*.
These attributes are mandatory and must contain the document-handle
of the from and to vertices of an edge.

Use the general document
[REST api](document-working-with-documents.html)
for create/read/update/delete.

<!-- Rest/Graph edges -->
{% docublock get_read_in_out_edges %}
