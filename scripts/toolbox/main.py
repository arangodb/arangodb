#!/usr/bin/env python

from modules import ArgumentParser
from modules import TestExecutor
import inspect
import config as cfg
import modules.ArangoDBInstanceManager as ArangoDBInstanceManager


def main():
    args = ArgumentParser.arguments()
    calculatedConfig = cfg.CalculatedConfig(args).getConfig()
    process = ArangoDBInstanceManager.start(args, calculatedConfig)
    TestExecutor.start(args, calculatedConfig)

    print("=> All tests finished")
    if inspect.isclass(process):
        # A process is returned in the single-server case
        ArangoDBInstanceManager.stop(process)
    else:
        # In cluster case, we do not return a process as we do have multiple.
        # Internally, we'll make use of our scripts/startLocalCluster.sh and
        # scripts/shutdownLocalCluster.sh scripts.
        ArangoDBInstanceManager.stopCluster(calculatedConfig)


if __name__ == "__main__":
    main()
