#!/usr/bin/env python

import importlib.util


def module_from_file(module_name, file_path):
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


Feed = module_from_file("Feed", "modules/Feed.py")


def start(args, cfg):
    # While ArangoDB is running, populate the database with data (100m documents)
    operations = '''[
      normal create database=xyz collection=c numberOfShards=3 replicationFactor=2
      normal insert database=xyz collection=c parallelism=10  batchSize=5000 size=128000K documentSize=128
    ]'''
    process = Feed.start(cfg, operations)
    print('--> Started test: "RocksDBCacheSizeTest". Output file is: "' + cfg.feed["jsonOutputFile"] + '"')
    return process
