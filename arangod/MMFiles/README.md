MMFilesEngine
=============

How operations are stored - Overview
------------------------------------

All operations like insert or remove are written as markers to a write ahead
log (WAL). This WAL consists of files of a certain size and if such a file is
full (or is manually flushed), all relevant markers are transferred
(transferMarkers()) to the journals of the respective collections. During the
transfer any obsolete markers will be thrown away: a sequence of insert, remove,
insert on the same document will result in the last insert discarding the
previous operations. When a journal file of size (journalSize()) is full, it
will be sealed and renamed. By applying these operations it will become a
datafile that is read-only. Datafiles will eventually be merged by a compactor
thread that does about the same work as the transferMarkers function, reducing
the size of the stored data.

Ditches
-------

Ditches are used to pin objects in WAL or journal as long as they are used by
other operations.
