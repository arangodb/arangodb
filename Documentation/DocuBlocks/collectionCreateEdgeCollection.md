

@brief creates a new edge collection
`db._createEdgeCollection(collection-name)`

Creates a new edge collection named *collection-name*. If the
collection name already exists an error is thrown. The default value
for *waitForSync* is *false*.

`db._createEdgeCollection(collection-name, properties)`

*properties* must be an object with the following attributes:

* *waitForSync* (optional, default *false*): If *true* creating
  a document will only return after the data was synced to disk.

