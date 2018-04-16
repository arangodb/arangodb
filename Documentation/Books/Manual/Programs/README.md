ArangoDB Programs
=================

The full ArangoDB package comes with the following programs:

- `arangod`: [ArangoDB server](../Administration/Configuration/GeneralArangod.md).
  This server program is intended to run as a daemon process / service to serve the
  various clients connections to the server via TCP / HTTP. It also provides a
  [web interface](../Administration/WebInterface/README.md).

- `arangodb`: [ArangoDB Starter](Starter/README.md) for easy deployment of
  ArangoDB instances.

- `arangosh`: [ArangoDB shell](../Administration/Arangosh/README.md).
  A client that implements a read-eval-print loop (REPL) and provides functions
  to access and administrate the ArangoDB server.

- `arangoimport`: [Bulk importer](../Administration/Arangoimport.md) for the
  ArangoDB server. It supports JSON and CSV.

- `arangoexport`: [Bulk exporter](../Administration/Arangoexport.md) for the
  ArangoDB server. It supports JSON, CSV and XML.

- `arangodump`: Tool to [create backups](../Administration/Arangodump.md)
  of an ArangoDB database in JSON format.

- `arangorestore`: Tool to [load data of a backup](../Administration/Arangorestore.md)
  back into an ArangoDB database.

- `arango-dfdb`: [Datafile debugger](../Troubleshooting/DatafileDebugger.md) for
  ArangoDB (MMFiles storage engine only). It is primarily intended to be used
  during development of ArangoDB.

- `arangobench`: [Benchmark and test tool](../Troubleshooting/Arangobench.md).
  It can be used for performance and server function testing.

- `arangovpack`: Utility to convert [VPack](https://github.com/arangodb/velocypack)
  data to JSON.

The client package comes with a subset of programs:

- arangosh
- arangoimport
- arangoexport
- arangodump
- arangorestore
- arangobench
- arangovpack
