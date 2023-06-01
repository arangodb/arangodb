#!/usr/bin/env python

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

feed = {
    "endpoints": "http://" + arangodb["host"] + ":" + arangodb["port"],
    "jsonOutputFile": "work/feed.json",
    "password": arangodb["passwd"],
    "user": arangodb["user"]
}
