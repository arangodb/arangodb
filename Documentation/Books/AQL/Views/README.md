Views in AQL
============

Conceptually a **view** is just another document data source, similar to an
array or a document/edge collection. The only distinction is that when querying
views the **VIEW** keyword must be specified in the **FOR** statement just
before the view name, e.g.:

FOR doc IN  exampleView
  FILTER ...
  SORT ...
  RETURN ...

A view is meant to be an abstraction over a transformation applied to documents
of zero or more collections. The transformation is view-implementation specific
and may even be as simple as an identity transformation thus making the view
represent all documents available in the specified set of collections.

Views can be defined and administered on a per view-type basis via
the [web interface](../../Manual/Programs/WebInterface/index.html).

The currently supported view implementations are:

- **arangosearch** as described in [ArangoSearch View](ArangoSearch/README.md)
