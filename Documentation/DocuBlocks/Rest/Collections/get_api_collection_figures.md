
@startDocuBlock get_api_collection_figures
@brief Fetch the statistics of a collection

@RESTHEADER{GET /_api/collection/{collection-name}/figures, Return statistics for a collection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
In addition to the above, the result also contains the number of documents
and additional statistical information about the collection.
**Note** : This will always load the collection into memory.

**Note**: collection data that are stored in the write-ahead log only are
not reported in the results. When the write-ahead log is collected, documents
might be added to journals and datafiles of the collection, which may modify
the figures of the collection.

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

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns information about the collection:

@RESTREPLYBODY{count,integer,required,int64}
The number of documents currently present in the collection.

@RESTREPLYBODY{figures,object,required,collection_figures}
metrics of the collection

@RESTSTRUCT{alive,collection_figures,object,required,collection_figures_alive}
the currently active figures

@RESTSTRUCT{count,collection_figures_alive,integer,required,int64}
The number of currently active documents in all datafiles
and journals of the collection. Documents that are contained in the
write-ahead log only are not reported in this figure.

@RESTSTRUCT{size,collection_figures_alive,integer,required,int64}
The total size in bytes used by all active documents of
the collection. Documents that are contained in the write-ahead log only are
not reported in this figure.

@RESTSTRUCT{dead,collection_figures,object,required,collection_figures_dead}
the items waiting to be swept away by the cleaner

@RESTSTRUCT{count,collection_figures_dead,integer,required,int64}
The number of dead documents. This includes document
versions that have been deleted or replaced by a newer version. Documents
deleted or replaced that are contained the write-ahead log only are not reported
in this figure.

@RESTSTRUCT{size,collection_figures_dead,integer,required,int64}
The total size in bytes used by all dead documents.

@RESTSTRUCT{deletion,collection_figures_dead,integer,required,int64}
The total number of deletion markers. Deletion markers
only contained in the write-ahead log are not reporting in this figure.

@RESTSTRUCT{datafiles,collection_figures,object,required,collection_figures_datafiles}
Metrics regarding the datafiles

@RESTSTRUCT{count,collection_figures_datafiles,integer,required,int64}
The number of datafiles.

@RESTSTRUCT{fileSize,collection_figures_datafiles,integer,required,int64}
The total filesize of datafiles (in bytes).

@RESTSTRUCT{journals,collection_figures,object,required,collection_figures_journals}
Metrics regarding the journal files

@RESTSTRUCT{count,collection_figures_journals,integer,required,int64}
The number of journal files.

@RESTSTRUCT{fileSize,collection_figures_journals,integer,required,int64}
The total filesize of all journal files (in bytes).

@RESTSTRUCT{compactors,collection_figures,object,required,collection_figures_compactors}

@RESTSTRUCT{count,collection_figures_compactors,integer,required,int64}
The number of compactor files.

@RESTSTRUCT{fileSize,collection_figures_compactors,integer,required,int64}
The total filesize of all compactor files (in bytes).

@RESTSTRUCT{readcache,collection_figures,object,required,collection_figures_readcache}

@RESTSTRUCT{count,collection_figures_readcache,integer,required,int64}
The number of revisions of this collection stored in the document revisions cache.

@RESTSTRUCT{size,collection_figures_readcache,integer,required,int64}
The memory used for storing the revisions of this collection in the document 
revisions cache (in bytes). This figure does not include the document data but 
only mappings from document revision ids to cache entry locations.

@RESTSTRUCT{revisions,collection_figures,object,required,collection_figures_revisions}

@RESTSTRUCT{count,collection_figures_revisions,integer,required,int64}
The number of revisions of this collection managed by the storage engine.

@RESTSTRUCT{size,collection_figures_revisions,integer,required,int64}
The memory used for storing the revisions of this collection in the storage 
engine (in bytes). This figure does not include the document data but only mappings 
from document revision ids to storage engine datafile positions.

@RESTSTRUCT{indexes,collection_figures,object,required,collection_figures_indexes}
@RESTSTRUCT{count,collection_figures_indexes,integer,required,int64}
The total number of indexes defined for the collection, including the pre-defined
indexes (e.g. primary index).

@RESTSTRUCT{size,collection_figures_indexes,integer,required,int64}
The total memory allocated for indexes in bytes.

@RESTSTRUCT{lastTick,collection_figures,integer,required,int64}
The tick of the last marker that was stored in a journal
of the collection. This might be 0 if the collection does not yet have
a journal.

@RESTSTRUCT{uncollectedLogfileEntries,collection_figures,integer,required,int64}
The number of markers in the write-ahead
log for this collection that have not been transferred to journals or datafiles.

@RESTSTRUCT{documentReferences,collection_figures,integer,optional,int64}
The number of references to documents in datafiles that JavaScript code 
currently holds. This information can be used for debugging compaction and 
unload issues.

@RESTSTRUCT{waitingFor,collection_figures,string,optional,string}
An optional string value that contains information about which object type is at the 
head of the collection's cleanup queue. This information can be used for debugging 
compaction and unload issues.

@RESTSTRUCT{compactionStatus,collection_figures,object,optional,compactionStatus_attributes}
@RESTSTRUCT{message,compactionStatus_attributes,string,optional,string}
The action that was performed when the compaction was last run for the collection. 
This information can be used for debugging compaction issues.

@RESTSTRUCT{time,compactionStatus_attributes,string,optional,string}
The point in time the compaction for the collection was last executed. 
This information can be used for debugging compaction issues.

@RESTREPLYBODY{journalSize,integer,required,int64}
The maximal size of a journal or datafile in bytes.

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Using an identifier and requesting the figures of the collection:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionFigures}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    coll.save({"test":"hello"});
    require("internal").wal.flush(true, true);
    var url = "/_api/collection/"+ coll.name() + "/figures";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

