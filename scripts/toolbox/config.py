#!/usr/bin/env python

# ArangoDB based configuration parameters
arangodb = {
    "host": "localhost",
    "port": "8529",
    "user": "root",
    "passwd": "",
    "startupParameters": {
        "server.authentication": "false",
        "log.file": "work/arangodb.log",
        "working-directory": "work",
        "database.directory": "work/database",
        "javascript.app-path": "../../js/apps",
        "javascript.module-directory": "../../enterprise/js",
        "javascript.startup-directory": "../../js"
    },
    "executable": "../../build/bin/arangod"
}

# ArangoDB Feed tool based configuration parameters
feed = {
    "endpoints": "http://" + arangodb["host"] + ":" + arangodb["port"],
    "jsonOutputFile": "work/feed.json",
    "password": arangodb["passwd"],
    "user": arangodb["user"]
}
