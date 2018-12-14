ArangoDB Production Checklist
=============================

The following checklist can help to understand if important steps
have been performed on your production system before you go live:

- Executed the OS optimization scripts (if you run ArangoDB on Linux).
  See [Installing ArangoDB on Linux](../Installation/Linux.md) for details.
- OS monitoring is in place
  (most common metrics, e.g. disk, CPU, RAM utilization).
- The user _root_ is not used to run any ArangoDB processes
  (if you run ArangoDB on Linux).
- Disk space monitoring is in place
  (only if you use the RocksDB storage engine)
- The _arangod_ (server) process and the _arangodb_ (Starter) process
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
