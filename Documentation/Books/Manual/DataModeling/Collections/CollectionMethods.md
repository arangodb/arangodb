Collection Methods
==================

Drop
----

<!-- arangod/V8Server/v8-collection.cpp -->


drops a collection
`collection.drop(options)`

Drops a *collection* and all its indexes and data.
In order to drop a system collection, an *options* object
with attribute *isSystem* set to *true* must be specified.

**Note**: dropping a collection in a cluster, which is prototype for
sharing in other collections is prohibited. In order to be able to
drop such a collection, all dependent collections must be dropped
first. 

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

    @startDocuBlockInline collectionDropSystem
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDropSystem}
    ~ db._create("_example", { isSystem: true });
      col = db._example;
      col.drop({ isSystem: true });
      col;
    ~ db._drop("example", { isSystem: true });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDropSystem


Truncate
--------

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


Compact
-------

<!-- js/server/modules/@arangodb/arango-collection.js-->

<small>Introduced in: v3.4.5</small>

Compacts the data of a collection
`collection.compact()`

Compacts the data of a collection in order to reclaim disk space. For the
MMFiles storage engine, the operation will reset the collection's last
compaction timestamp, so it will become a candidate for compaction. For the
RocksDB storage engine, the operation will compact the document and index
data by rewriting the underlying .sst files and only keeping the relevant
entries.

Under normal circumstances running a compact operation is not necessary,
as the collection data will eventually get compacted anyway. However, in 
some situations, e.g. after running lots of update/replace or remove 
operations, the disk data for a collection may contain a lot of outdated data
for which the space shall be reclaimed. In this case the compaction operation
can be used.


Properties
----------

<!-- arangod/V8Server/v8-collection.cpp -->

@startDocuBlock collectionProperties

Figures
-------

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
* *lastTick*: The tick of the last marker that was stored in a journal
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
and reflect the logical file sizes. Some filesystems may use optimizations
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


GetResponsibleShard
-------------------

<!-- arangod/V8Server/v8-collection.cpp -->


returns the responsible shard for the given document.
`collection.getResponsibleShard(document)`

Returns a string with the responsible shard's ID. Note that the
returned shard ID is the ID of responsible shard for the document's
shard key values, and it will be returned even if no such document exists.

**Note**: this function can only be used on a coordinator in a cluster.


Load
----

<!-- arangod/V8Server/v8-collection.cpp -->


loads a collection
`collection.load()`

Loads a collection into memory.

**Note**: cluster collections are loaded at all times.

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


Revision
--------

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


Path
----


returns the physical path of the collection
`collection.path()`

The *path* operation returns a string with the physical storage path for
the collection data.

**Note**: this method will return nothing meaningful in a cluster. In a 
single-server ArangoDB, this method will only return meaningful data for the 
MMFiles engine.



Checksum
--------

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


Unload
------

<!-- arangod/V8Server/v8-collection.cpp -->


unloads a collection
`collection.unload()`

Starts unloading a collection from memory. Note that unloading is deferred
until all query have finished.

**Note**: cluster collections cannot be unloaded.

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


Rename
------

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


Rotate
------

<!-- arangod/V8Server/v8-collection.cpp -->


rotates the current journal of a collection
`collection.rotate()`

Rotates the current journal of a collection. This operation makes the
current journal of the collection a read-only datafile so it may become a
candidate for garbage collection. If there is currently no journal available
for the collection, the operation will fail with an error.

**Note**: this method is specific for the MMFiles storage engine, and there
it is not available in a cluster.

**Note**: please note that you need appropriate user permissions to execute this. 
 - To do the rename collections in first place you need to have administrative rights on the database
 - To have access to the resulting renamed collection you either need to have access to 
   all collections of that database (`*`) or a main system administrator has to give you access to 
   the newly named one.

