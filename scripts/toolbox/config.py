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
        "configuration": "../../etc/relative/arangod.conf",
        "log.file": globals["workDir"] + "/arangodb.log",
        "working-directory": globals["workDir"],
        "database.directory": globals["workDir"] + "/database",
        "javascript.app-path": "../../js/apps",
        "javascript.module-directory": "../../enterprise/js",
        "javascript.startup-directory": "../../js"
    },
    "executable": "../../build/bin/arangod",
    "pocDirectory": "../../tests/js/transform/poc/"
}

# ArangoDB Feed tool based configuration parameters
feed = {
    "endpoints": "http://" + arangodb["host"] + ":" + arangodb["port"],
    "jsonOutputFile": globals["workDir"] + "/feed.json",
    "password": arangodb["passwd"],
    "user": arangodb["user"]
}

# ArangoDB Metrics based configuration parameters
metrics = {
    "interval": 1,
    "endpoint": "http://" + arangodb["host"] + ":" + arangodb["port"] + "/_admin/metrics"
}

hotBackup = {
    "workDir": globals["workDir"] + "/hotBackup"
}


class CalculatedConfig:
    def __init__(self, args):
        self.arangodb = arangodb
        self.feed = feed
        self.globals = globals
        self.metrics = metrics
        self.hotBackup = hotBackup

        if args.host and args.port:
            self.arangodb["port"] = args.port
            self.arangodb["host"] = args.host
            self.arangodb["startupParameters"]["server.endpoint"] = "tcp://" + args.host + ":" + args.port
            self.feed["endpoints"] = "http://" + args.host + ":" + self.arangodb["port"]
            self.metrics["endpoint"] = "http://" + args.host + ":" + self.arangodb["port"] + "/_admin/metrics"

        # TODO: This can be done better, but for now it works
        if args.workDir:
            self.arangodb["startupParameters"]["working-directory"] = args.workDir
            self.arangodb["startupParameters"]["log.file"] = args.workDir + "/arangodb.log"
            self.arangodb["startupParameters"]["database.directory"] = args.workDir + "/database"
            self.feed["jsonOutputFile"] = args.workDir + "/feed.json"
            self.globals["workDir"] = args.workDir
            self.hotBackup["workDir"] = args.workDir + "/hotBackup"

    def getConfig(self):
        return {
            "arangodb": self.arangodb,
            "feed": self.feed,
            "globals": self.globals,
            "metrics": self.metrics
        }

    def getArangoDB(self):
        return self.arangodb

    def getFeed(self):
        return self.feed

    def getGlobals(self):
        return self.globals

    def getMetrics(self):
        return self.metrics

    def getHotBackup(self):
        return self.hotBackup
