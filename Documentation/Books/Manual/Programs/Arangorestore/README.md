Arangorestore
=============

Arangorestore is a command-line client tool to restore backups created by
[Arangodump](../Arangodump/README.md) to [ArangoDB servers](../Arangod/README.md).

If you want to import data in formats like JSON or CSV, see
[Arangoimport](../Arangoimport/README.md) instead.

Arangorestore can restore selected collections or all collections of a backup,
optionally including system collections. One can restore the structure, i.e.
the collections with their configuration with or without data.
