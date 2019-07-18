

@brief returns the figures of a collection
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
* *readCache.count*: The number of revisions of this collection stored
  in the document revisions cache.
* *readCache.size*: The memory used for storing the revisions 
  of this collection in the document revisions cache (in bytes). This 
  figure does not include the document data but only mappings from
  document revision ids to cache entry locations.
* *revisions.count*: The number of revisions of this collection managed
  by the storage engine.
* *revisions.size*: The memory used for storing the revisions 
  of this collection in the storage engine (in bytes). This figure
  does not include the document data but only mappings from
  document revision ids to storage engine datafile positions.
* *indexes.count*: The total number of indexes defined for the
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

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionFigures_mmfiles}
~ require("internal").wal.flush(true, true);
  db.demo.figures()
@END_EXAMPLE_ARANGOSH_OUTPUT


