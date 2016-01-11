

@brief ignore datafile errors when loading collections
`--database.ignore-datafile-errors boolean`

If set to `false`, CRC mismatch and other errors in collection datafiles
will lead to a collection not being loaded at all. The collection in this
case becomes unavailable. If such collection needs to be loaded during WAL
recovery, the WAL recovery will also abort (if not forced with option
`--wal.ignore-recovery-errors true`).

Setting this flag to `false` protects users from unintentionally using a
collection with corrupted datafiles, from which only a subset of the
original data can be recovered. Working with such collection could lead
to data loss and follow up errors.
In order to access such collection, it is required to inspect and repair
the collection datafile with the datafile debugger (arango-dfdb).

If set to `true`, CRC mismatch and other errors during the loading of a
collection will lead to the datafile being partially loaded, up to the
position of the first error. All data up to until the invalid position
will be loaded. This will enable users to continue with collection
datafiles
even if they are corrupted, but this will result in only a partial load
of the original data and potential follow up errors. The WAL recovery
will still abort when encountering a collection with a corrupted datafile,
at least if `--wal.ignore-recovery-errors` is not set to `true`.

The default value is *false*, so collections with corrupted datafiles will
not be loaded at all, preventing partial loads and follow up errors.
However,
if such collection is required at server startup, during WAL recovery, the
server will abort the recovery and refuse to start.

