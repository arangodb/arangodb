Collection Methods
==================

### Drop
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionDrop

### Truncate
<!-- js/server/modules/org/arangodb/arango-collection.js-->
@startDocuBlock collectionTruncate

### Properties
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionProperties

### Figures
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionFigures

### Load
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionLoad

### Reserve
`collection.reserve( number)`

Sends a resize hint to the indexes in the collection. The resize hint allows indexes to reserve space for additional documents (specified by number) in one go.

The reserve hint can be sent before a mass insertion into the collection is started. It allows indexes to allocate the required memory at once and avoids re-allocations and possible re-locations.

Not all indexes implement the reserve function at the moment. The indexes that don't implement it will simply ignore the request. returns the revision id of a collection

### Revision
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionRevision

### Checksum
<!-- arangod/V8Server/v8-query.cpp -->
@startDocuBlock collectionChecksum

### Unload
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionUnload

### Rename
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionRename

### Rotate
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionRotate
