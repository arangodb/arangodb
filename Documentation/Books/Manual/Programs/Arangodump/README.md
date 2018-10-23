Arangodump
==========

_Arangodump_ is a command-line client tool to create backups of the data and
structures stored in ArangoDB.

Dumps are meant to be restored with [_Arangorestore_](../Arangorestore/README.md).

If you want to export for external programs to formats like JSON or CSV, see
[_Arangoexport_](../Arangoexport/README.md) instead.

_Arangodump_ can be used for all ArangoDB deployments modes (Single Instance, 
Master/Slave, Active Failover, Cluster and DC2DC) and it can backup selected collections
or all collections of a database, optionally including _system_ collections. One
can backup the structure, i.e. the collections with their configuration without
any data, only the data stored in them, or both. If you are using the Enterprise
Edition, dumps can optionally be encrypted.
