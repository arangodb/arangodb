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


@contextmanager
def set_env(**environ):
    """
    Temporarily set the process environment variables.

    >>> with set_env(PLUGINS_DIR='test/plugins'):
    ...   "PLUGINS_DIR" in os.environ
    True

    >>> "PLUGINS_DIR" in os.environ
    False

    :type environ: dict[str, unicode]
    :param environ: Environment variables to set
    """
    oldEnviron = dict(os.environ)
    os.environ.update(environ)
    try:
        yield
    finally:
        os.environ.clear()
        os.environ.update(oldEnviron)


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
        startCluster()
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
    elif args.mode == "custom":
        print("ArangoDB will not be started automatically!")
    else:
        print("Invalid mode")


class InstanceOptions:
    defaultWorkDir = "../../"

    def __init__(self):
        self.workDir = self.defaultWorkDir
        self.initialize = True

    def setWorkDir(self, workDir):
        self.workDir = workDir

    def setInitialize(self, initialize):
        self.initialize = initialize


def startCluster(options=None, performUpgrade=False):
    if options is None:
        options = InstanceOptions()
    else:
        assert (isinstance(options, InstanceOptions))

    print("Starting ArangoDB in cluster mode...")
    with cwd(options.workDir):
        print("WorkDir is: " + options.workDir)
        if options.initialize:
            try:
                shutil.rmtree("cluster")
                shutil.rmtree("cluster-init")
            except FileNotFoundError:
                pass

        oldEnvironment = dict(os.environ)
        oldEnvironment = {"AUTOUPGRADE": "0"}
        if performUpgrade:
            oldEnvironment = {"AUTOUPGRADE": "1"}

        # Set upgrade environment variable
        os.environ.update(oldEnvironment)
        parameters = ["./scripts/startLocalCluster.sh", "-b", "-1"]
        subprocess.Popen(parameters).wait()


def stopCluster(options=None):
    if options is None:
        options = InstanceOptions()
    else:
        assert (isinstance(options, InstanceOptions))

    print("Stopping ArangoDB in cluster mode...")
    # main ArangoDB repository
    with cwd(options.workDir):
        parameters = ["./scripts/shutdownLocalCluster.sh"]
        subprocess.Popen(parameters).wait()


def copyDataDirectory(oldOptions, newOptions):
    print("Copying data directory from " + oldOptions.workDir + " to " + newOptions.workDir)

    try:
        shutil.rmtree(newOptions.workDir + "/cluster-init")
    except FileNotFoundError:
        pass

    shutil.copytree(oldOptions.workDir + "/cluster", newOptions.workDir + "/cluster-init")


# Currently forced to work with the scripts/startLocalCluster.sh script
# and two source directories (old and new, ArangoDB src checked-out from git)
def upgradeCluster(oldOptions, newOptions):
    assert (isinstance(oldOptions, InstanceOptions))
    assert (isinstance(newOptions, InstanceOptions))

    # 1.) Stop cluster "old"
    stopCluster(oldOptions)
    # 2.) Copy "old" cluster directory to "new" cluster directory
    copyDataDirectory(oldOptions, newOptions)
    # 3.) Start cluster "new" with upgrade procedure
    startCluster(newOptions, performUpgrade=True)
    # 4.) Start cluster "new" again with default procedure
    startCluster(newOptions, performUpgrade=False)


def stopAndWaitCluster():
    stopCluster()
    time.sleep(5)  # TODO: wait for process to be shut down properly


def stop(process):
    print("Stopping ArangoDB...")
    process.kill()
