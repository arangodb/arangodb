Programs & Tools
================

The full ArangoDB package ships with the following programs and tools:

| Binary name     | Brief description |
|-----------------|-------------------|
| `arangod`       | [ArangoDB server](Arangod.md). This server program is intended to run as a daemon process / service to serve the various client connections to the server via TCP / HTTP. It also provides a [web interface](WebInterface/README.md).
| `arangodb`      | [ArangoDB Starter](Starter/README.md) for easy deployment of ArangoDB instances.
| `arangosh`      | [ArangoDB shell](Arangosh/README.md). A client that implements a read-eval-print loop (REPL) and provides functions to access and administrate the ArangoDB server.
| `arangoimport`  | [Bulk importer](Arangoimport/README.md) for the ArangoDB server. It supports JSON and CSV.
| `arangoexport`  | [Bulk exporter](Arangoexport/README.md) for the ArangoDB server. It supports JSON, CSV and XML.
| `arangodump`    | Tool to [create backups](Arangodump/README.md) of an ArangoDB database in JSON format.
| `arangorestore` | Tool to [load data of a backup](Arangorestore/README.md) back into an ArangoDB database.
| `arango-dfdb`   | [Datafile debugger](DatafileDebugger.md) for ArangoDB (MMFiles storage engine only). It is primarily intended to be used during development of ArangoDB.
| `arangobench`   | [Benchmark and test tool](Arangobench/README.md). It can be used for performance and server function testing.
| `arangovpack`   | Utility to convert [VelocyPack](https://github.com/arangodb/velocypack) data to JSON. 

The client package comes with a subset of programs and tools:

- arangosh
- arangoimport
- arangoexport
- arangodump
- arangorestore
- arangobench
- arangovpack
