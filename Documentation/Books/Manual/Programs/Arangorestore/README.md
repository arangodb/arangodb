Arangorestore
=============

_Arangorestore_ is a command-line client tool to restore backups created by
[_Arangodump_](../Arangodump/README.md) to
[ArangoDB servers](../Arangod/README.md).

If you want to import data in formats like JSON or CSV, see
[_Arangoimport_](../Arangoimport/README.md) instead.

_Arangorestore_ can restore selected collections or all collections of a backup,
optionally including _system_ collections. One can restore the structure, i.e.
the collections with their configuration with or without data.
Views can also be dumped or restored (either all of them or selectively).