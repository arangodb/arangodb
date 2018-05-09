Arangodump
==========

Arangodump is a command-line client tool to create backups of the data and
structures stored in [ArangoDB servers](../Arangod/README.md).

Dumps are meant to be restored with [Arangorestore](../Arangorestore/README.md).

If you want to export for external programs to formats like JSON or CSV, see
[Arangoexport](../Arangoexport/README.md) instead.

Arangodump can backup selected collections or all collections of a database,
optionally including system collections. One can backup the structure, i.e.
the collections with their configuration without any data, only the data stored
in them, or both. Dumps can optionally be encrypted.
