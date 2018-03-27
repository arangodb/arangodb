Arangorestore Options
=====================

Usage: `arangorestore [<options>]`

Global configuration
--------------------

```
  --batch-size <uint64>                              maximum size for individual data batches (in bytes) (default:
                                                     8388608)
  --collection <string...>                           restrict to collection name (can be specified multiple times)
                                                     (default: )
  --configuration <string>                           the configuration file or 'none' (default: "")
  --create-collection <boolean>                      create collection structure (default: true)
  --create-database <boolean>                        create the target database if it does not exist (default: false)
  --default-number-of-shards <uint64>                default value for numberOfShards if not specified (default: 1)
  --default-replication-factor <uint64>              default value for replicationFactor if not specified (default: 1)
  --force <boolean>                                  continue restore even in the face of some server-side errors
                                                     (default: false)
  --force-same-database <boolean>                    force usage of the same database name as in the source dump.json
                                                     file (default: false)
  --ignore-distribute-shards-like-errors <boolean>   continue restore even if sharding prototype collection is missing
                                                     (default: false)
  --import-data <boolean>                            import data into collection (default: true)
  --include-system-collections <boolean>             include system collections (default: false)
  --input-directory <string>                         input directory (default: "C:\Program Files\ArangoDB3 3.3.3\dump")
  --overwrite <boolean>                              overwrite collections if they exist (default: true)
  --progress <boolean>                               show progress (default: true)
  --version <boolean>                                reports the version and exits (default: false)
```

Configure the logging
---------------------

```
  --log.color <boolean>                              use colors for TTY logging (default: true)
  --log.level <string...>                            the global or topic-specific log level (default: "info")
  --log.output <string...>                           log destination(s) (default: )
  --log.role <boolean>                               log server role (default: false)
  --log.use-local-time <boolean>                     use local timezone instead of UTC (default: false)
  --log.use-microtime <boolean>                      use microtime instead (default: false)
```

Configure a connection to the server
------------------------------------

```
  --server.authentication <boolean>                  require authentication credentials when connecting (does not
                                                     affect the server-side authentication settings) (default: true)
  --server.connection-timeout <double>               connection timeout in seconds (default: 5)
  --server.database <string>                         database name to use when connecting (default: "_system")
  --server.endpoint <string>                         endpoint to connect to, use 'none' to start without a server
                                                     (default: "http+tcp://127.0.0.1:8529")
  --server.password <string>                         password to use when connecting. If not specified and
                                                     authentication is required, the user will be prompted for a
                                                     password (default: "")
  --server.request-timeout <double>                  request timeout in seconds (default: 1200)
  --server.username <string>                         username to use when connecting (default: "root")
```

Configure SSL communication
---------------------------

```
  --ssl.protocol <uint64>                            ssl protocol (1 = SSLv2, 2 = SSLv2 or SSLv3 (negotiated), 3 =
                                                     SSLv3, 4 = TLSv1, 5 = TLSV1.2). possible values: 1, 2, 3, 4, 5
                                                     (default: 5)
```

Configure temporary files
-------------------------

```
  --temp.path <string>                               path for temporary files (default: "")
```
