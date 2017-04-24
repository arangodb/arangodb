

@brief returns the revision id of a collection
`collection.revision()`

Returns the revision id of the collection

The revision id is updated when the document data is modified, either by
inserting, deleting, updating or replacing documents in it.

The revision id of a collection can be used by clients to check whether
data in a collection has changed or if it is still unmodified since a
previous fetch of the revision id.

The revision id returned is a string value. Clients should treat this value
as an opaque string, and only use it for equality/non-equality comparisons.

**Note**: this method will produce different output under the RocksDB storage engine with regard to `remove` and `truncate` when compared with the MMFiles storage engine.
