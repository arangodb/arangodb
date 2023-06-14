#!/usr/bin/env python

import time
import os
import shutil
import subprocess
import json
from contextlib import contextmanager


@contextmanager
def cwd(path):
    oldPwd = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(oldPwd)


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
        startCluster(cfg)
    elif args.mode == "single":
        parameters = [cfg["arangodb"]["executable"]]
        for key, value in cfg["arangodb"]["startupParameters"].items():
            parameters.append("--" + key + "=" + value)

        if args.startupParameters:
            additionalStartupParameters = json.loads(args.startupParameters)
            for key, value in additionalStartupParameters.items():
                parameters.append("--" + key + "=" + value)

        process = subprocess.Popen(parameters, stdout=subprocess.DEVNULL)
        print("We will wait 5 seconds for arangod to start up...")
        time.sleep(5)  # TODO: wait for process to be started up properly
        print(
            "Started ArangoDB in single mode, logfile: " + cfg["arangodb"]["startupParameters"][
                "log.file"] + ", port: " +
            cfg["arangodb"]["port"] + " (silently!)")
        return process
    else:
        print("Invalid mode")


def startCluster(cfg):
    print("Starting ArangoDB in cluster mode...")
    # main ArangoDB repository
    with cwd('../../'):
        parameters = ["./scripts/startLocalCluster.sh"]
        subprocess.Popen(parameters).wait()


def stopCluster(cfg):
    print("Stopping ArangoDB in cluster mode...")
    # main ArangoDB repository
    with cwd('../../'):
        parameters = ["./scripts/shutdownLocalCluster.sh"]
        subprocess.Popen(parameters).wait()


def stopAndWaitCluster(cfg):
    stopCluster(cfg)
    time.sleep(5)  # TODO: wait for process to be shut down properly


def stop(process):
    print("Stopping ArangoDB...")
    process.kill()
