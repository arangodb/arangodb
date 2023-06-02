#!/usr/bin/env python

from modules import ArgumentParser
from modules import TestExecutor
import config as cfg
import modules.ArangoDBStarter as ArangoDBStarter


def main():
    args = ArgumentParser.arguments()
    calculatedConfig = cfg.CalculatedConfig(args).getConfig()
    process = ArangoDBStarter.start(args, calculatedConfig)
    testProcess = TestExecutor.start(args, calculatedConfig)
    testProcess.wait()
    print("=> All tests finished")
    ArangoDBStarter.stop(process)


if __name__ == "__main__":
    main()
