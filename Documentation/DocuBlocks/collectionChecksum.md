

@brief calculates a checksum for the data in a collection
`collection.checksum(withRevisions, withData)`

The *checksum* operation calculates a CRC32 checksum of the keys
contained in collection *collection*.

If the optional argument *withRevisions* is set to *true*, then the
revision ids of the documents are also included in the checksumming.

If the optional argument *withData* is set to *true*, then the
actual document data is also checksummed. Including the document data in
checksumming will make the calculation slower, but is more accurate.

**Note**: this method is not available in a cluster.


