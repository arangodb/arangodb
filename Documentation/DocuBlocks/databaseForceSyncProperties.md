
@startDocuBlock databaseForceSyncProperties
@brief force syncing of collection properties to disk
`--database.force-sync-properties boolean`

Force syncing of collection properties to disk after creating a collection
or updating its properties.

If turned off, no fsync will happen for the collection and database
properties stored in `parameter.json` files in the file system. Turning
off this option will speed up workloads that create and drop a lot of
collections (e.g. test suites).

The default is *true*.
@endDocuBlock

