Programs & Tools
================

The full ArangoDB package ships with the following programs and tools:

| Binary name     | Brief description |
|-----------------|-------------------|
| `arangod`       | [ArangoDB server](Arangod/README.md). This server program is intended to run as a daemon process / service to serve the various client connections to the server via TCP / HTTP. It also provides a [web interface](WebInterface/README.md).
| `arangosh`      | [ArangoDB shell](Arangosh/README.md). A client that implements a read-eval-print loop (REPL) and provides functions to access and administrate the ArangoDB server.
| `arangodb`      | [ArangoDB Starter](Starter/README.md) for easy deployment of ArangoDB instances.
| `arangodump`    | Tool to [create backups](Arangodump/README.md) of an ArangoDB database.
| `arangorestore` | Tool to [load backups](Arangorestore/README.md) back into an ArangoDB database.
| `arangoimport`  | [Bulk importer](Arangoimport/README.md) for the ArangoDB server. It supports JSON and CSV.
| `arangoexport`  | [Bulk exporter](Arangoexport/README.md) for the ArangoDB server. It supports JSON, CSV and XML.
| `arango-dfdb`   | [Datafile debugger](Arango-dfdb/README.md) for ArangoDB (MMFiles storage engine only).
| `arangobench`   | [Benchmark and test tool](Arangobench/README.md). It can be used for performance and server function testing.
| `arangoinspect` | [Inspection tool](Arangoinspect/README.md) that gathers server setup information.
| `arangovpack`   | Utility to convert [VelocyPack](https://github.com/arangodb/velocypack) data to JSON.

The client package comes with a subset of programs and tools:

- arangosh
- arangoimport
- arangoexport
- arangodump
- arangorestore
- arangobench
- arangoinspect
- arangovpack

Additional tools which are available separately:

| Name            | Brief description |
|-----------------|-------------------|
| [Foxx CLI](FoxxCLI/README.md) | Command line tool for managing and developing Foxx services
| [kube-arangodb](../Deployment/Kubernetes/README.md) | Operators to manage Kubernetes deployments
