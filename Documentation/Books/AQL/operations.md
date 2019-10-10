---
layout: default
description: High-level operations are the core language constructs of the query language.
title: AQL High-level Operations
---
High-level operations
=====================

The following high-level operations are described here after:

- [**FOR**](operations-for.html):
  Iterate over a collection or View, all elements of an array or traverse a graph

- [**RETURN**](operations-return.html):
  Produce the result of a query.

- [**FILTER**](operations-filter.html):
  Restrict the results to elements that match arbitrary logical conditions.

- [**SEARCH**](operations-search.html):
  Query the (full-text) index of an ArangoSearch View

- [**SORT**](operations-sort.html):
  Force a sort of the array of already produced intermediate results.

- [**LIMIT**](operations-limit.html):
  Reduce the number of elements in the result to at most the specified number,
  optionally skip elements (pagination).

- [**LET**](operations-let.html):
  Assign an arbitrary value to a variable.

- [**COLLECT**](operations-collect.html):
  Group an array by one or multiple group criteria. Can also count and aggregate.

- [**REMOVE**](operations-remove.html):
  Remove documents from a collection.

- [**UPDATE**](operations-update.html):
  Partially update documents in a collection.

- [**REPLACE**](operations-replace.html):
  Completely replace documents in a collection.

- [**INSERT**](operations-insert.html):
  Insert new documents into a collection.

- [**UPSERT**](operations-upsert.html):
  Update/replace an existing document, or create it in the case it does not exist.

- [**WITH**](operations-with.html):
  Specify collections used in a query (at query begin only).
