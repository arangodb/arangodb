ArangoDB Production Checklist
=============================

The following checklist can help understanding if some important steps 
have been performed on your production system:

- you have executed the OS optimization scripts (if you are running ArangoDB on Linux). Please refer to the [Installing ArangoDB on Linux](../Installation/Linux.md) section
- you have OS monitoring in place (most common metrics, e.g. disk, CPU, RAM utilization, etc)
- you are not using the user _root_ to run your ArangoDB processes (if you are running on Linux)
- if you are using RocksDB, you have disk space monitoring in place
- your _arangod_ processes as well as _arangodb_ processes (if in use) are writing some logs and you can easily locate and inspect them
- if you are using the _Starter_ to deploy, you have stopped - and disabled automated start of - the ArangoDB _Single Instance_, e.g. on _Ubuntu_ `service arangodb3 stop` and `update-rc.d -f arangodb3 remove`
