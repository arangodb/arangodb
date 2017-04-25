

@brief rotates the current journal of a collection
`collection.rotate()`

Rotates the current journal of a collection. This operation makes the
current journal of the collection a read-only datafile so it may become a
candidate for garbage collection. If there is currently no journal available
for the collection, the operation will fail with an error.

**Note**: this method is only available when using the MMFiles storage engine.
**Note**: this method is not available in a cluster.
