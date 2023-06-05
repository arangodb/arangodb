#!/usr/bin/env python

from tests import RocksDBCacheSizeTest
from modules import MetricsTracker


def start(args, cfg):
    if (args.devFlag):
        mt = MetricsTracker.MetricsTracker(cfg, [])
        mt.start()
        RocksDBCacheSizeTest.start(args, cfg).wait()
        mt.stopAndWrite("RocksDBCacheSizeTest")
    else:
        RocksDBCacheSizeTest.start(args, cfg).wait()
