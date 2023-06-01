#!/usr/bin/env python

import time
import os
import shutil
import subprocess
import json


def start(args, cfg):
    if args.init == "true":
        try:
            os.remove(cfg.arangodb["startupParameters"]["log.file"])
            shutil.rmtree(cfg.arangodb["startupParameters"]["database.directory"])
        except Exception:
            pass

    if args.mode == "cluster":
        print("Starting ArangoDB in cluster mode")
    elif args.mode == "single":
        print("Starting ArangoDB in single mode, logfile: " + cfg.arangodb["startupParameters"]["log.file"])
        parameters = [cfg.arangodb["executable"]]
        for key, value in cfg.arangodb["startupParameters"].items():
            parameters.append("--" + key + "=" + value)

        if args.startupParameters:
            additionalStartupParameters = json.loads(args.startupParameters)
            for key, value in additionalStartupParameters.items():
                parameters.append("--" + key + "=" + value)

        process = subprocess.Popen(parameters)
        time.sleep(5)  # TODO: wait for process to be started up properly
        return process
    else:
        print("Invalid mode")


def stop(process):
    print("Stopping ArangoDB")
    process.kill()
