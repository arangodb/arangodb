

@brief creates a new edge collection
`db._createEdgeCollection(collection-name)`

Creates a new edge collection named *collection-name*. If the
collection name already exists an error is thrown. The default value
for *waitForSync* is *false*.

`db._createEdgeCollection(collection-name, properties)`

*properties* must be an object with the following attributes:

* *waitForSync* (optional, default *false*): If *true* creating
  a document will only return after the data was synced to disk.
* *journalSize* (optional, default is 
  "configuration parameter"):  The maximal size of
  a journal or datafile.  Note that this also limits the maximal
  size of a single object and must be at least 1MB.


