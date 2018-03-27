Arangoimport Options
====================

Usage: `arangoimp [<options>]`

Global configuration
--------------------

```
  --backslash-escape <boolean>           use backslash as the escape character for quotes, used for csv (default: false)
  --batch-size <uint64>                  size for individual data batches (in bytes) (default: 16777216)
  --collection <string>                  collection name (default: "")
  --configuration <string>               the configuration file or 'none' (default: "")
  --convert <boolean>                    convert the strings 'null', 'false', 'true' and strings containing numbers into non-string types (csv and tsv only) (default: true)
  --create-collection <boolean>          create collection if it does not yet exist (default: false)
  --create-collection-type <string>      type of collection if collection is created (edge or document). possible values: "document", "edge" (default: "document")
  --file <string>                        file name ("-" for STDIN) (default: "")
  --from-collection-prefix <string>      _from collection name prefix (will be prepended to all values in '_from') (default: "")
  --ignore-missing <boolean>             ignore missing columns in csv input (default: false)
  --on-duplicate <string>                action to perform when a unique key constraint violation occurs. Possible values: error, update, ignore, replace. possible values: "error", "ignore", "replace", "update" (default: "error")
  --overwrite <boolean>                  overwrite collection if it exist (WARNING: this will remove any data from the collection) (default: false)
  --progress <boolean>                   show progress (default: true)
  --quote <string>                       quote character(s), used for csv (default: """)
  --remove-attribute <string...>         remove an attribute before inserting an attribute into a collection (for csv and tsv only) (default: )
  --separator <string>                   field separator, used for csv and tsv (default: "")
  --skip-lines <uint64>                  number of lines to skip for formats (csv and tsv only) (default: 0)
  --threads <uint32>                     Number of parallel import threads. Most useful for the rocksdb engine (default: 2)
  --to-collection-prefix <string>        _to collection name prefix (will be prepended to all values in '_to') (default: "")
  --translate <string...>                translate an attribute name (use as --translate "from=to", for csv and tsv only) (default: )
  --type <string>                        type of import file. possible values: "auto", "csv", "json", "jsonl", "tsv" (default: "json")
  --version <boolean>                    reports the version and exits (default: false)
```

Configure the logging
---------------------

```
  --log.color <boolean>                  use colors for TTY logging (default: true)
  --log.level <string...>                the global or topic-specific log level (default: "info")
  --log.output <string...>               log destination(s) (default: )
  --log.role <boolean>                   log server role (default: false)
  --log.use-local-time <boolean>         use local timezone instead of UTC (default: false)
  --log.use-microtime <boolean>          use microtime instead (default: false)
```

Configure a connection to the server
------------------------------------

```
  --server.authentication <boolean>      require authentication credentials when connecting (does not affect the server-side authentication settings) (default: true)
  --server.connection-timeout <double>   connection timeout in seconds (default: 5)
  --server.database <string>             database name to use when connecting (default: "_system")
  --server.endpoint <string>             endpoint to connect to, use 'none' to start without a server (default: "http+tcp://127.0.0.1:8529")
  --server.password <string>             password to use when connecting. If not specified and authentication is required, the user will be prompted for a password (default: "")
  --server.request-timeout <double>      request timeout in seconds (default: 1200)
  --server.username <string>             username to use when connecting (default: "root")
```

Configure SSL communication
---------------------------

```
  --ssl.protocol <uint64>                ssl protocol (1 = SSLv2, 2 = SSLv2 or SSLv3 (negotiated), 3 = SSLv3, 4 = TLSv1, 5 = TLSV1.2). possible values: 1, 2, 3, 4, 5 (default: 5)
```

Configure temporary files
-------------------------

```
  --temp.path <string>                   path for temporary files (default: "")
```
