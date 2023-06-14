#!/usr/bin/env python

import time
from arango import ArangoClient
from modules import HotBackup


def start(args, cfg):
    endpoint = "http://localhost:8530"
    # Steps do be done here:
    # - Create an additional database next to _system
    # - Check that the "_pregel_queries" system collection is present in:
    #   - "_system"
    #   - "the additional database"
    # - Remove both pregel system collections out of both databases
    # - Create a hot backup of the ArangoDB instance
    # - Restore the hot backup
    #   - We expect, that the "_pregel_queries" system collection is not present again in both databases
    print("Starting HotBackup Test")
    additionalDatabaseName = "additionalDatabase"
    systemDatabaseName = "_system"
    pregelSystemCollectionName = "_pregel_queries"

    client = ArangoClient(hosts=endpoint)
    sysDB = client.db(systemDatabaseName)
    sysDB.create_database(additionalDatabaseName)

    assert (sysDB.has_collection(pregelSystemCollectionName))
    assert (sysDB.has_database(additionalDatabaseName))
    testDB = client.db(additionalDatabaseName)
    assert (testDB.has_collection(pregelSystemCollectionName))

    sysDB.delete_collection(pregelSystemCollectionName, system=True)
    testDB.delete_collection(pregelSystemCollectionName, system=True)

    hotBackupLabel = "the_name_is_CLUSTER"
    hotBackupIdentifier = HotBackup.createHotBackup(endpoint, hotBackupLabel)
    print("My hot backup identifier is: " + hotBackupIdentifier)
    HotBackup.restoreHotBackup(endpoint, hotBackupIdentifier)

    assert (sysDB.has_collection(pregelSystemCollectionName))
    assert (testDB.has_collection(pregelSystemCollectionName))

    print("Done HotBackup Test")
