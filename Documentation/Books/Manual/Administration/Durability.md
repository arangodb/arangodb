Durability Configuration
========================

Global Configuration
--------------------

There are global configuration values for durability, which can be adjusted by
specifying the following configuration options:

@startDocuBlock databaseWaitForSync


@startDocuBlock databaseForceSyncProperties


@startDocuBlock WalLogfileSyncInterval


Per-collection configuration
----------------------------

You can also configure the durability behavior on a per-collection basis.
Use the ArangoDB shell to change these properties.


@startDocuBlock collectionProperties


Per-operation configuration
---------------------------

Many data-modification operations and also ArangoDB's transactions allow to specify 
a *waitForSync* attribute, which when set ensures the operation data has been 
synchronized to disk when the operation returns.

Disk-Usage Configuration
------------------------

The amount of disk space used by ArangoDB is determined by a few configuration
options. 

Global Configuration
--------------------

The total amount of disk storage required by ArangoDB is determined by the size of
the write-ahead logfiles plus the sizes of the collection journals and datafiles.

There are the following options for configuring the number and sizes of the write-ahead
logfiles:

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileReserveLogfiles


<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileHistoricLogfiles


<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSize


<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileAllowOversizeEntries


When data gets copied from the write-ahead logfiles into the journals or datafiles
of collections, files will be created on the collection level. How big these files
are is determined by the following global configuration value:

<!-- arangod/RestServer/ArangoServer.h -->
@startDocuBlock databaseMaximalJournalSize


Per-collection configuration
----------------------------

The journal size can also be adjusted on a per-collection level using the collection's
*properties* method.
