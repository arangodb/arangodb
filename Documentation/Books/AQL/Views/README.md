Views in AQL
============

Conceptually a **view** is just another document data source, similar to an
array or a document/edge collection, e.g.:

```js
FOR doc IN exampleView SEARCH ...
  FILTER ...
  SORT ...
  RETURN ...
```

Other than collections, views have an additional but optional `SEARCH` keyword:

```js
FOR doc IN exampleView
  SEARCH ...
  FILTER ...
  SORT ...
  RETURN ...
```

A view is meant to be an abstraction over a transformation applied to documents
of zero or more collections. The transformation is view-implementation specific
and may even be as simple as an identity transformation thus making the view
represent all documents available in the specified set of collections.

Views can be defined and administered on a per view-type basis via
the [web interface](../../Manual/Programs/WebInterface/index.html).

Currently there is a single supported view implementation, namely
`arangosearch` as described in [ArangoSearch View](ArangoSearch/README.md). 

Also see the detailed
[ArangoSearch tutorial](https://www.arangodb.com/tutorials/arangosearch/)
to learn more.
