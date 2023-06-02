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


class CalculatedConfig:
    def __init__(self, args):
        self.arangodb = arangodb
        self.feed = feed
        self.globals = globals

        # TODO: This can be done better, but for now it works
        if args.workDir:
            self.arangodb["startupParameters"]["working-directory"] = args.workDir
            self.arangodb["startupParameters"]["log.file"] = args.workDir + "/arangodb.log"
            self.arangodb["startupParameters"]["database.directory"] = args.workDir + "/database"
            self.feed["jsonOutputFile"] = args.workDir + "/feed.json"
            self.globals["workDir"] = args.workDir

    def getConfig(self):
        return {
            "arangodb": self.arangodb,
            "feed": self.feed,
            "globals": self.globals
        }

    def getArangoDB(self):
        return self.arangodb

    def getFeed(self):
        return self.feed

    def getGlobals(self):
        return self.globals
