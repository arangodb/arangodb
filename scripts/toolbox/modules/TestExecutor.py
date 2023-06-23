#!/usr/bin/env python

from tests import RocksDBCacheSizeTest
from tests import HotBackupSystemCollectionRestoreTest
from modules import MetricsTracker
from modules import Feed

# TODO: TO BE REMOVED SOON and replaced by a better runner.

def start(args, cfg):
    if args.devFlag:
        # This area is for development purposes only. It is not used in production.
        # It helps to only run a single test and not all together.
        print("Starting in dev mode")
        # HotBackupSystemCollectionRestoreTest.start(args, cfg)
        Feed.startFromFile(cfg, cfg["arangodb"]["pocDirectory"] + "input.feed")

        print("Done (dev mode)")
    else:
        # Right now this section starts manual selected tests. This is WIP. In the future, this should be done by
        # a test selector.
        mt = MetricsTracker.MetricsTracker(cfg, ["rocksdb"])
        mt.start()
        RocksDBCacheSizeTest.start(args, cfg).wait()
        mt.stopAndWrite("RocksDBCacheSizeTest")
