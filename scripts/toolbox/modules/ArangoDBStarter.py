#!/usr/bin/env python

import time
import os
import shutil
import subprocess
import json


def checkWorkDirectory(args, cfg):
    if args.workDir:
        if not os.path.exists(args.workDir):
            os.makedirs(args.workDir)
    else:
        if not os.path.exists(cfg["globals"]["workDir"]):
            os.makedirs(cfg["globals"]["workDir"])


def start(args, cfg):
    if args.init == "true":
        try:
            os.remove(cfg["arangodb"]["startupParameters"]["log.file"])
            shutil.rmtree(cfg["arangodb"]["startupParameters"]["database.directory"])
        except Exception:
            pass

    checkWorkDirectory(args, cfg)

    if args.mode == "cluster":
        print("TODO: Not yet implemented <Starting ArangoDB in cluster mode>")
    elif args.mode == "single":
        parameters = [cfg["arangodb"]["executable"]]
        for key, value in cfg["arangodb"]["startupParameters"].items():
            parameters.append("--" + key + "=" + value)

        if args.startupParameters:
            additionalStartupParameters = json.loads(args.startupParameters)
            for key, value in additionalStartupParameters.items():
                parameters.append("--" + key + "=" + value)

        process = subprocess.Popen(parameters, stdout=subprocess.DEVNULL)
        time.sleep(5)  # TODO: wait for process to be started up properly
        print(
            "Started ArangoDB in single mode, logfile: " + cfg["arangodb"]["startupParameters"][
                "log.file"] + ", port: " +
            cfg["arangodb"]["port"] + " (silently!)")
        return process
    else:
        print("Invalid mode")


def stop(process):
    print("Stopping ArangoDB...")
    process.kill()
