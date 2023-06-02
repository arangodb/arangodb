#!/usr/bin/env python

globals = {
    "workDir": "work"
}

# ArangoDB based configuration parameters
arangodb = {
    "host": "localhost",
    "port": "8529",
    "user": "root",
    "passwd": "",
    "startupParameters": {
        "server.authentication": "false",
        "log.file": globals["workDir"] + "/arangodb.log",
        "working-directory": globals["workDir"],
        "database.directory": globals["workDir"] + "/database",
        "javascript.app-path": "../../js/apps",
        "javascript.module-directory": "../../enterprise/js",
        "javascript.startup-directory": "../../js"
    },
    "executable": "../../build/bin/arangod"
}

# ArangoDB Feed tool based configuration parameters
feed = {
    "endpoints": "http://" + arangodb["host"] + ":" + arangodb["port"],
    "jsonOutputFile": globals["workDir"] + "/feed.json",
    "password": arangodb["passwd"],
    "user": arangodb["user"]
}
