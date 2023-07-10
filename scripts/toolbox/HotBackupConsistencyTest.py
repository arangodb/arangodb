#!/usr/bin/env python

import threading
import argparse
import tempfile
import time
import os
import sys

from pathlib import Path
from termcolor import colored
from arango import ArangoClient
from modules import ArangoDBInstanceManager
from modules.ArangodEnvironment.ArangodEnvironment import ArangodEnvironment
from modules.ArangodEnvironment.Agent import Agent
from modules.ArangodEnvironment.Coordinator import Coordinator
from modules.ArangodEnvironment.DBServer import DBServer

def inserter(collection):
    for i in range(10000):
        collection.insert({"number": i, "field1": "stone"})

def start_cluster(binary, topdir, workdir):
    environment = ArangodEnvironment(binary, topdir, workdir)
    environment.start_agent()
    environment.start_coordinator()

    for i in range(2):
        environment.start_dbserver()

    return environment

def ensure_workdir(desired_workdir):
    if desired_workdir is None:
        return tempfile.mkdtemp(prefix="arangotest_")
    else:
        if os.path.exists(desired_workdir):
            print(f"ERROR: desired working directory {desired_workdir} already exists")
            sys.exit(-1)
        else:
            path = Path(desired_workdir)
            path.mkdir(parents=True)
            return(desired_workdir)

def run_test(client):
    # create a database to run the test on
    sys_db = client.db('_system', username='root')
    sys_db.create_database('test')

    test_db = client.db('test', username='root')

    # collection into which documents will be inserted and indexed
    test_collection = test_db.create_collection('test_collection')
    index = test_collection.add_inverted_index(fields=["field1"])
    test_db.create_view(name="test_view", view_type="arangosearch",
                        properties = { "links": {
                            "test_collection": {
                                "includeAllFields": True }}})

    test_collection2 = test_db.create_collection('test_collection2')
    test_db.create_view(name="test_view_squared", view_type="arangosearch",
                        properties = { "links": {
                            "test_collection2": {
                                "includeAllFields": True }}})

    # Start a transaction
    txn_db = test_db.begin_transaction(read="test_collection2", write="test_collection2")
    txn_col = txn_db.collection("test_collection2")
    txn_col.insert({"foo": "bar"})

    # Insert documents into test_collection
    insjob = threading.Thread(target=inserter, args=[test_collection])
    insjob.start()
    # :/ making sure that something has happened from the
    # inserter thread
    time.sleep(1)

    # Concurrently, take a hotbackup
    result = sys_db.backup.create(
        label="hotbackuptesthotbackup",
        allow_inconsistent=False,
        force=False,
        timeout=1000
    )
    print(f"backup_id: {result['backup_id']}")

    # wait for the insertion job to finish
    # TODO:
    #  * have the inserter just continously insert documents
    #  * signal for it to stop here
    #  * join it.
    insjob.join()

    # restore the hotbackup taken above
    result = sys_db.backup.restore(result["backup_id"])

    # Check that all documents that are in the test_view
    # are also in the test_collection (and vice-versa)
    p = list(test_db.aql.execute(
        """LET x = (FOR v IN test_collection RETURN v._key)
           FOR w IN test_view
             FILTER w._key NOT IN x
             RETURN w
        """))
    assert(p == [])
    q = list(test_db.aql.execute(
        """LET x = (FOR v IN test_view RETURN v._key)
           FOR w IN test_collection
             FILTER w._key NOT IN x
             RETURN w
        """))
    assert q == [], f"q == {q}"

    # Check that the inverted index is consistent with the documents
    # in the collection.
    # I could not come up with a reliable way to do this: the AQL optimiser
    # is free to optimise the index into the first query, and then
    # this check is void. in the same way it could happen that the optimiser
    # decidesd that the filter in the second query is not best served by
    # the inverted index and the test is void.
    p = set(test_db.aql.execute("FOR v IN test_collection RETURN v._key"))
    q = set(test_db.aql.execute("""
        FOR w IN test_collection
             FILTER w.field1 == "stone"
             RETURN w._key
        """))
    assert p == q

    txn_db.abort_transaction()


def main():
    parser = argparse.ArgumentParser(
        prog="HotBackupConsistencyTest.py",
        description="tests hotbackups",
        epilog="")
    parser.add_argument('--arangod', required=True,
                        help="path to the arangod executable to use")
    parser.add_argument('--topdir', required=True,
                        help=("the top of the arangodb directory; "
                              "paths to the js/ and enterprise/js "
                              "folders are derived from this"))
    parser.add_argument('--workdir', default=None,
                        help=("working directory; all databases, "
                              "logfiles, and backups will be stored "
                              " there"))
    args = parser.parse_args()

    workdir = ensure_workdir(args.workdir)
    print(f"working directory is {workdir}")

    environment = start_cluster(args.arangod, args.topdir, workdir)

    # TODO: replace this with API call for cluster readiness
    time.sleep(15)
    client = ArangoClient(environment.get_coordinator_http_endpoint())

    run_test(client)

if __name__ == "__main__":
    main()
