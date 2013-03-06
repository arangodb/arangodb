Using Locks in ArangoDB
=======================

This documents summarizes the various locks and their usage. You should
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

TODO

TRI_*_DOCUMENTS_INDEXES_PRIMARY_COLLECTION (R/W)
================================================

TODO
