#!/usr/bin/env python

from modules import ArgumentParser
from modules import TestExecutor
import config as cfg
import modules.ArangoDBStarter as ArangoDBStarter


def main():
    args = ArgumentParser.arguments()
    process = ArangoDBStarter.start(args, cfg)
    TestExecutor.start(args, cfg)
    ArangoDBStarter.stop(process)


main()
