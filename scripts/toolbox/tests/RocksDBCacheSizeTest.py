#!/usr/bin/env python

from modules import Feed


def start(args, cfg):
    # While ArangoDB is running, populate the database with data (100m documents)

    # SMALL
    # normal insert database=xyz collection=c parallelism=10  batchSize=5000 size=128000K documentSize=128
    # BIG
    # normal insert database=xyz collection=c parallelism=10  batchSize=5000 size=10G

    operations = '''[
      normal create database=xyz collection=c numberOfShards=3 replicationFactor=2
      normal insert database=xyz collection=c parallelism=10  batchSize=5000 size=10G
    ]'''
    process = Feed.start(cfg, operations)
    print('--> Started test: "RocksDBCacheSizeTest". Output file is: "' + cfg["feed"]["jsonOutputFile"] + '"')
    return process
