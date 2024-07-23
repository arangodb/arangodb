#!/usr/bin/env python

from modules import Feed


def start(args, cfg):
    # While ArangoDB is running, populate the database with data pre-defined feeds:

    smallSimpleOperations = '''[
      normal create database=xyz collection=c numberOfShards=3 replicationFactor=2
      normal insert database=xyz collection=c parallelism=10  batchSize=5000 size=1G
    ]'''

    bigSimpleOperations = '''[
      normal create database=xyz collection=c numberOfShards=3 replicationFactor=2
      normal insert database=xyz collection=c parallelism=10  batchSize=5000 size=10G
    ]'''

    smallAdvancedOperations = '''[
      # Create a collection
      normal create database=xyz collection=c numberOfShards=3 replicationFactor=2
      normal insert database=xyz collection=c size=1G documentSize=300 withGeo=false withWords=0 parallelism=64 numberFields=5
      
      # Drop stuff again:
      normal drop database=xyz collection=c
      normal dropDatabase database=xyz
      
      # Now two collections, half the data each:
      normal create database=xyz collection=d numberOfShards=3 replicationFactor=2 drop=true
      normal create database=xyz collection=e numberOfShards=3 replicationFactor=2 drop=true

      # Insert data concurrently:
      {
        normal insert database=xyz collection=d size=500M documentSize=300 withGeo=false withWords=0 parallelism=64 numberFields=5
        normal insert database=xyz collection=e size=500M documentSize=300 withGeo=false withWords=0 parallelism=64 numberFields=5
      }
      
      # Replace with concurrent random reads:
      {
        normal randomReplace database=xyz collection=d parallelism=64 batchSize=1000 loadPerThread=20
        normal randomRead database=xyz collection=e parallelism=64 loadPerThread=100
      }

      # Update with concurrent random reads:
      {
        normal randomUpdate database=xyz collection=d parallelism=64 batchSize=1000 loadPerThread=20
        normal randomRead database=xyz collection=e parallelism=64 loadPerThread=100
      }
    ]'''

    bigAdvancedOperations = '''[
          # Create a collection
          normal create database=xyz collection=c numberOfShards=3 replicationFactor=2
          normal insert database=xyz collection=c size=10G documentSize=300 withGeo=false withWords=0 parallelism=64 numberFields=5

          # Drop stuff again:
          normal drop database=xyz collection=c
          normal dropDatabase database=xyz

          # Now two collections, half the data each:
          normal create database=xyz collection=d numberOfShards=3 replicationFactor=2 drop=true
          normal create database=xyz collection=e numberOfShards=3 replicationFactor=2 drop=true

          # Insert data concurrently:
          {
            normal insert database=xyz collection=d size=5G documentSize=300 withGeo=false withWords=0 parallelism=64 numberFields=5
            normal insert database=xyz collection=e size=5G documentSize=300 withGeo=false withWords=0 parallelism=64 numberFields=5
          }

          # Replace with concurrent random reads:
          {
            normal randomReplace database=xyz collection=d parallelism=64 batchSize=1000 loadPerThread=20
            normal randomRead database=xyz collection=e parallelism=64 loadPerThread=100
          }

          # Update with concurrent random reads:
          {
            normal randomUpdate database=xyz collection=d parallelism=64 batchSize=1000 loadPerThread=20
            normal randomRead database=xyz collection=e parallelism=64 loadPerThread=100
          }
        ]'''

    process = Feed.start(cfg, bigAdvancedOperations)
    print('--> Started test: "RocksDBCacheSizeTest". Output file is: "' + cfg["feed"]["jsonOutputFile"] + '"')
    return process
