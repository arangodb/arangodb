---
layout: default
description: The following checklist can help to understand if important stepshave been performed on your production system before you go live
---
ArangoDB Production Checklist
=============================

The following checklist can help to understand if important steps
have been performed on your production system before you go live:

- Executed the OS optimization scripts (if you run ArangoDB on Linux).
  See [Installing ArangoDB on Linux](installation-linux.html) for details.
- OS monitoring is in place
  (most common metrics, e.g. disk, CPU, RAM utilization).
- Disk space monitoring is in place
  (only if you use the RocksDB storage engine).
- The user _root_ is not used to run any ArangoDB processes
  (if you run ArangoDB on Linux).
- The _arangod_ (server) process and the _arangodb_ (_Starter_) process
  (if in use) have some form of logging enabled and you can easily
  locate and inspect them.
- If you use the _Starter_ to deploy, you stopped - and disabled
  automated start of - the ArangoDB _Single Instance_, e.g. on Ubuntu:

  ```
  service arangodb3 stop
  update-rc.d -f arangodb3 remove
  ```

  On Windows in a command prompt with elevated rights:

  ```
  sc config arangodb start= disabled
  sc stop arangodb
  ```

- If you have deployed a Cluster (and/or are using DC2DC) the 
  _replication factor_ of your collections is set to a value equal
  or higher than 2 (so that you have **minimal
  _data redundancy_ in place**).
