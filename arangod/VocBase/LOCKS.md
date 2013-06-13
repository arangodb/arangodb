Using Locks in ArangoDB
=======================

This document summarizes the various locks and their usage. You should
carefully read it in order to avoid dead-locks.

TRI_*_COLLECTIONS_VOCBASE (R/W)
===============================

Origin: `TRI_vocbase_t`, attribute `_lock`.

A vocbase contains collections, there names, and identifiers. Whenever you need
to access these variables you need a read lock. Whenever you need to modify
these variables you need a write lock.

If you need a TRI_*_COLLECTIONS_VOCBASE and a TRI_*_STATUS_VOCBASE_COL, you must
first grab the TRI_*_STATUS_VOCBASE_COL and then the TRI_*_COLLECTIONS_VOCBASE.

Protected attributes of `TRI_vocbase_t`:

- `_collections`
- `_deadCollections`
- `_collectionsByName`
- `_collectionsById`

TRI_*_STATUS_VOCBASE_COL (R/W)
==============================

Origin: `TRI_vocbase_col_t`, attribute `_lock`

This lock protects the status (loaded, unloaded) of the collection.

If you want to use a collection, you must call `TRI_UseCollectionVocBase`. This
will either load or manifest the collection and a read-lock is held when the
functions returns.  You must call `TRI_ReleaseCollectionVocBase`, when you
finished using the collection. The functions that modify the status of
collection (load, unload, manifest) must grab a write-lock.

If you need a TRI_*_COLLECTIONS_VOCBASE and a TRI_*_STATUS_VOCBASE_COL, you must
first grab the TRI_*_STATUS_VOCBASE_COL and then the TRI_*_COLLECTIONS_VOCBASE.

Protected attributes of `TRI_vocbase_col_t`:

- `_status`
- `_collection`
- `_name`
- `_path`


TRI_*_SYNCHRONISER_WAITER_VOC_BASE (C)
======================================

TODO

TRI_*_JOURNAL_ENTRIES_DOC_COLLECTION (L)
========================================

TODO

TRI_*_DATAFILES_DOC_COLLECTION (R/W)
====================================

primaryCollection->_lock
Note: this is the same lock as DOCUMENTS_INDEXES_PRIMARY_COLLECTION
TODO

TRI_*_DOCUMENTS_INDEXES_PRIMARY_COLLECTION (R/W)
================================================

primaryCollection->_lock
Note: this is the same lock as DATAFILES_DOC_COLLECTION
TODO

COMPACTION LOCK (R/W)
=====================

For each document collection, there is a lock protecting the compaction.
Though there is at most one compaction thread and thus only a single 
compaction action going on at a time, this lock is required.
When the compactor thread checks whether a collection needs compaction,
it will try to acquire the collection's compaction write lock.
If this lock cannot be acquired instantly, the collection will not be
collected in this run.
If the lock can be acquired, a datafile of the collection can be compacted.

The compaction will modify both datafile statistics and header information,
and these operations must be safe-guarded and cannot run in parallel with
other that read/modify the same data.

When a write-transaction is executed, it will acquire the compaction lock
of each participating collection in read-mode. If the compaction is 
currently going on for a collection, the transaction will wait until the
compaction for the collection is finished.

The intention is to run either the compaction or write operations in a 
collection, but not both at the same time.
