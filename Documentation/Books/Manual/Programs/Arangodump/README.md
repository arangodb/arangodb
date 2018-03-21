Arangodump
==========

Arangodump is a client tool to create backups of the data and structures stored in [ArangoDB servers](../Arangod/README.md).

Dumps are meant to be restored with [Arangorestore](../Arangorestore/README.md).

If you want to export to formats like CSV instead, see [Arangoexport]()

Arangodump can backup selected collections or all collections of a database, optionally including system collections. One can backup the structure, i.e. the collections with their configuration without data, only the data stored in them, or both. Dumps can optionally be encrypted.