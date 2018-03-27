Arangodump Options
==================

Usage: `arangodump [<options>]`

Global configuration
--------------------

```
  --batch-size <uint64>                              maximum size for individual data batches (in bytes) (default:
                                                     67108864)
  --collection <string...>                           restrict to collection name (can be specified multiple times)
                                                     (default: )
  --configuration <string>                           the configuration file or 'none' (default: "")
  --dump-data <boolean>                              dump collection data (default: true)
  --force <boolean>                                  continue dumping even in the face of some server-side errors
                                                     (default: false)
  --ignore-distribute-shards-like-errors <boolean>   continue dump even if sharding prototype collection is not backed
                                                     up along (default: false)
  --include-system-collections <boolean>             include system collections (default: false)
  --initial-batch-size <uint64>                      initial size for individual data batches (in bytes) (default:
                                                     8388608)
  --output-directory <string>                        output directory (default: "C:\Program Files\ArangoDB3 3.3.
                                                     3\dump")
  --overwrite <boolean>                              overwrite data in output directory (default: false)
  --progress <boolean>                               show progress (default: true)
  --tick-end <uint64>                                last tick to be included in data dump (default: 0)
  --tick-start <uint64>                              only include data after this tick (default: 0)
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

Section 'ssl' (Configure SSL communication)
  --ssl.protocol <uint64>                            ssl protocol (1 = SSLv2, 2 = SSLv2 or SSLv3 (negotiated), 3 =
                                                     SSLv3, 4 = TLSv1, 5 = TLSV1.2). possible values: 1, 2, 3, 4, 5
                                                     (default: 5)
```
