#!/usr/bin/env python

from termcolor import colored
from arango import ArangoClient
from modules import HotBackup
from modules import ArangoDBInstanceManager


def start(args, cfg):
    assert (args.mode == "custom")
    print("Starting HotBackup Test")

    # Environment preparation (this should be located somewhere else, but for now it's fine here)
    # 1.) Start ArangoDB Cluster old version (3.10.X)
    oldOptions = ArangoDBInstanceManager.InstanceOptions()
    oldOptions.setWorkDir("../../../3.10")
    oldOptions.setInitialize(False)
    print(oldOptions.workDir)
    ArangoDBInstanceManager.startCluster(oldOptions)

    endpoint = "http://localhost:8530"
    # 1.) Start ArangoDB Cluster old version (3.10.X)
    # Steps do be done here:
    # - Create an additional database next to _system
    # - Check that the "_pregel_queries" system collection is NOT present in:
    #   - "_system"
    #   - "the additional database"
    # - Create a hot backup of the ArangoDB instance
    # 2.) Upgrade the cluster from old to new (3.11.X)
    # - Restore the hot backup
    #   - We expect, that the "_pregel_queries" system collection is not present again in both databases
    additionalDatabaseName = "additionalDatabase"
    systemDatabaseName = "_system"
    pregelSystemCollectionName = "_pregel_queries"

    client = ArangoClient(hosts=endpoint)
    sysDB = client.db(systemDatabaseName)
    sysDB.create_database(additionalDatabaseName)

    assert (not sysDB.has_collection(pregelSystemCollectionName))
    assert (sysDB.has_database(additionalDatabaseName))
    testDB = client.db(additionalDatabaseName)
    assert (not testDB.has_collection(pregelSystemCollectionName))


    hotBackupLabel = "the_name_is_CLUSTER"
    hotBackupIdentifier = HotBackup.createHotBackup(endpoint, hotBackupLabel)
    print("My hot backup identifier is: " + hotBackupIdentifier)

    # 2.) Upgrade the cluster from old to new (3.11.X || devel)
    newOptions = ArangoDBInstanceManager.InstanceOptions()
    newOptions.setInitialize(False)
    ArangoDBInstanceManager.upgradeCluster(oldOptions, newOptions)

    HotBackup.restoreHotBackup(endpoint, hotBackupIdentifier)

    try:
        assert (sysDB.has_collection(pregelSystemCollectionName))
        assert (testDB.has_collection(pregelSystemCollectionName))
    except AssertionError:
        # TODO: Add at a later point a global try catch handler for handling assertions.
        errorMessage = "ERROR: The pregel system collection is not present in both databases after the restore"
        print(colored("TEST FAILED", 'red'))
        print(colored(errorMessage, 'red'))
        print(colored("TEST FAILED", 'red'))
        exit(1)

    # Right now I do not want to exit in failure case, as it seems that I've found a shutdown issue
    # right after the restore of a HotBackup. This is WIP.
    print("Done HotBackup Test")
