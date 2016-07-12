!CHAPTER Collection Methods

!SUBSECTION Drop
<!-- arangod/V8Server/v8-collection.cpp -->


drops a collection
`collection.drop()`

Drops a *collection* and all its indexes.


**Examples**


    @startDocuBlockInline collectionDrop
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDrop}
    ~ db._create("example");
      col = db.example;
      col.drop();
      col;
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDrop



!SUBSECTION Truncate
<!-- js/server/modules/@arangodb/arango-collection.js-->


truncates a collection
`collection.truncate()`

Truncates a *collection*, removing all documents but keeping all its
indexes.


**Examples**


Truncates a collection:

    @startDocuBlockInline collectionTruncate
    @EXAMPLE_ARANGOSH_OUTPUT{collectionTruncate}
    ~ db._create("example");
      col = db.example;
      col.save({ "Hello" : "World" });
      col.count();
      col.truncate();
      col.count();
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionTruncate


!SUBSECTION Properties
<!-- arangod/V8Server/v8-collection.cpp -->
@startDocuBlock collectionProperties

!SUBSECTION Figures
<!-- arangod/V8Server/v8-collection.cpp -->


returns the figures of a collection
`collection.figures()`

Returns an object containing statistics about the collection.
**Note** : Retrieving the figures will always load the collection into 
memory.

* *alive.count*: The number of currently active documents in all datafiles and
  journals of the collection. Documents that are contained in the
  write-ahead log only are not reported in this figure.
* *alive.size*: The total size in bytes used by all active documents of the
  collection. Documents that are contained in the write-ahead log only are
  not reported in this figure.
- *dead.count*: The number of dead documents. This includes document
  versions that have been deleted or replaced by a newer version. Documents
  deleted or replaced that are contained in the write-ahead log only are not
  reported in this figure.
* *dead.size*: The total size in bytes used by all dead documents.
* *dead.deletion*: The total number of deletion markers. Deletion markers
  only contained in the write-ahead log are not reporting in this figure.
* *datafiles.count*: The number of datafiles.
* *datafiles.fileSize*: The total filesize of datafiles (in bytes).
* *journals.count*: The number of journal files.
* *journals.fileSize*: The total filesize of the journal files
  (in bytes).
* *compactors.count*: The number of compactor files.
* *compactors.fileSize*: The total filesize of the compactor files
  (in bytes).
* *shapefiles.count*: The number of shape files. This value is
  deprecated and kept for compatibility reasons only. The value will always
  be 0 since ArangoDB 2.0 and higher.
* *shapefiles.fileSize*: The total filesize of the shape files. This
  value is deprecated and kept for compatibility reasons only. The value will
  always be 0 in ArangoDB 2.0 and higher.
* *shapes.count*: The total number of shapes used in the collection.
  This includes shapes that are not in use anymore. Shapes that are contained
  in the write-ahead log only are not reported in this figure.
* *shapes.size*: The total size of all shapes (in bytes). This includes
  shapes that are not in use anymore. Shapes that are contained in the
  write-ahead log only are not reported in this figure.
* *attributes.count*: The total number of attributes used in the
  collection. Note: the value includes data of attributes that are not in use
  anymore. Attributes that are contained in the write-ahead log only are
  not reported in this figure.
* *attributes.size*: The total size of the attribute data (in bytes).
  Note: the value includes data of attributes that are not in use anymore.
  Attributes that are contained in the write-ahead log only are not 
  reported in this figure.
* *indexes.count*: The total number of indexes defined for the
  collection, including the pre-defined indexes (e.g. primary index).
* *indexes.size*: The total memory allocated for indexes in bytes.
* *maxTick*: The tick of the last marker that was stored in a journal
  of the collection. This might be 0 if the collection does not yet have
  a journal.
* *uncollectedLogfileEntries*: The number of markers in the write-ahead
  log for this collection that have not been transferred to journals or
  datafiles.
* *documentReferences*: The number of references to documents in datafiles
  that JavaScript code currently holds. This information can be used for
  debugging compaction and unload issues.
* *waitingFor*: An optional string value that contains information about
  which object type is at the head of the collection's cleanup queue. This 
  information can be used for debugging compaction and unload issues.
* *compactionStatus.time*: The point in time the compaction for the collection
  was last executed. This information can be used for debugging compaction
  issues.
* *compactionStatus.message*: The action that was performed when the compaction
  was last run for the collection. This information can be used for debugging
  compaction issues.

**Note**: collection data that are stored in the write-ahead log only are
not reported in the results. When the write-ahead log is collected, documents
might be added to journals and datafiles of the collection, which may modify 
the figures of the collection. Also note that `waitingFor` and `compactionStatus` 
may be empty when called on a coordinator in a cluster.

Additionally, the filesizes of collection and index parameter JSON files are
not reported. These files should normally have a size of a few bytes
each. Please also note that the *fileSize* values are reported in bytes
and reflect the logical file sizes. Some filesystems may use optimisations
(e.g. sparse files) so that the actual physical file size is somewhat
different. Directories and sub-directories may also require space in the
file system, but this space is not reported in the *fileSize* results.

That means that the figures reported do not reflect the actual disk
usage of the collection with 100% accuracy. The actual disk usage of
a collection is normally slightly higher than the sum of the reported 
*fileSize* values. Still the sum of the *fileSize* values can still be 
used as a lower bound approximation of the disk usage.


**Examples**


    @startDocuBlockInline collectionFigures
    @EXAMPLE_ARANGOSH_OUTPUT{collectionFigures}
    ~ require("internal").wal.flush(true, true);
      db.demo.figures()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionFigures



!SUBSECTION Load
<!-- arangod/V8Server/v8-collection.cpp -->


loads a collection
`collection.load()`

Loads a collection into memory.


**Examples**


    @startDocuBlockInline collectionLoad
    @EXAMPLE_ARANGOSH_OUTPUT{collectionLoad}
    ~ db._create("example");
      col = db.example;
      col.load();
      col;
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionLoad



!SUBSECTION Reserve
`collection.reserve(number)`

Sends a resize hint to the indexes in the collection. The resize hint allows indexes to reserve space for additional documents (specified by number) in one go.

The reserve hint can be sent before a mass insertion into the collection is started. It allows indexes to allocate the required memory at once and avoids re-allocations and possible re-locations.

Not all indexes implement the reserve function at the moment. The indexes that don't implement it will simply ignore the request. returns the revision id of a collection

!SUBSECTION Revision
<!-- arangod/V8Server/v8-collection.cpp -->


returns the revision id of a collection
`collection.revision()`

Returns the revision id of the collection

The revision id is updated when the document data is modified, either by
inserting, deleting, updating or replacing documents in it.

The revision id of a collection can be used by clients to check whether
data in a collection has changed or if it is still unmodified since a
previous fetch of the revision id.

The revision id returned is a string value. Clients should treat this value
as an opaque string, and only use it for equality/non-equality comparisons.


!SUBSECTION Checksum
<!-- arangod/V8Server/v8-query.cpp -->


calculates a checksum for the data in a collection
`collection.checksum(withRevisions, withData)`

The *checksum* operation calculates an aggregate hash value for all document
keys contained in collection *collection*.

If the optional argument *withRevisions* is set to *true*, then the
revision ids of the documents are also included in the hash calculation.

If the optional argument *withData* is set to *true*, then all user-defined
document attributes are also checksummed. Including the document data in
checksumming will make the calculation slower, but is more accurate.

The checksum calculation algorithm changed in ArangoDB 3.0, so checksums from
3.0 and earlier versions for the same data will differ.

**Note**: this method is not available in a cluster.


!SUBSECTION Unload
<!-- arangod/V8Server/v8-collection.cpp -->


unloads a collection
`collection.unload()`

Starts unloading a collection from memory. Note that unloading is deferred
until all query have finished.


**Examples**


    @startDocuBlockInline CollectionUnload
    @EXAMPLE_ARANGOSH_OUTPUT{CollectionUnload}
    ~ db._create("example");
      col = db.example;
      col.unload();
      col;
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock CollectionUnload



!SUBSECTION Rename
<!-- arangod/V8Server/v8-collection.cpp -->


renames a collection
`collection.rename(new-name)`

Renames a collection using the *new-name*. The *new-name* must not
already be used for a different collection. *new-name* must also be a
valid collection name. For more information on valid collection names please
refer to the [naming conventions](../NamingConventions/README.md).

If renaming fails for any reason, an error is thrown.
If renaming the collection succeeds, then the collection is also renamed in
all graph definitions inside the `_graphs` collection in the current
database.

**Note**: this method is not available in a cluster.


**Examples**


    @startDocuBlockInline collectionRename
    @EXAMPLE_ARANGOSH_OUTPUT{collectionRename}
    ~ db._create("example");
      c = db.example;
      c.rename("better-example");
      c;
    ~ db._drop("better-example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionRename



!SUBSECTION Rotate
<!-- arangod/V8Server/v8-collection.cpp -->


rotates the current journal of a collection
`collection.rotate()`

Rotates the current journal of a collection. This operation makes the
current journal of the collection a read-only datafile so it may become a
candidate for garbage collection. If there is currently no journal available
for the collection, the operation will fail with an error.

**Note**: this method is not available in a cluster.


