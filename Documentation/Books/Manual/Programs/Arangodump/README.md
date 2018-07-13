Arangodump
==========

_Arangodump_ is a command-line client tool to create backups of the data and
structures stored in [ArangoDB servers](../Arangod/README.md).

Dumps are meant to be restored with [_Arangorestore_](../Arangorestore/README.md).

If you want to export for external programs to formats like JSON or CSV, see
[_Arangoexport_](../Arangoexport/README.md) instead.

_Arangodump_ can backup selected collections or all collections of a database,
optionally including _system_ collections. One can backup the structure, i.e.
the collections with their configuration without any data, only the data stored
in them, or both. Dumps can optionally be encrypted.
