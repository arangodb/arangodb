#!/usr/bin/env python

from modules import ArgumentParser
from modules import TestExecutor
import config as cfg
import modules.ArangoDBStarter as ArangoDBStarter


def main():
    args = ArgumentParser.arguments()
    process = ArangoDBStarter.start(args, cfg)
    testProcess = TestExecutor.start(args, cfg)
    testProcess.wait()
    print("=> All tests finished")
    ArangoDBStarter.stop(process)


if __name__ == "__main__":
    main()
