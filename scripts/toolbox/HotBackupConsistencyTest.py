#!/usr/bin/env python

import threading
import argparse
import tempfile
import time
import os
import sys
import json

from pathlib import Path
from termcolor import colored
from arango import ArangoClient
from modules import ArangoDBInstanceManager
from modules.ArangodEnvironment.ArangodEnvironment import ArangodEnvironment
from modules.ArangodEnvironment.Agent import Agent
from modules.ArangodEnvironment.Coordinator import Coordinator
from modules.ArangodEnvironment.DBServer import DBServer


def inserter(collection, stop_ev):
    for i in range(10000):
        collection.insert({"number": i, "field1": "stone"})
        if stop_ev.is_set():
            break


def start_cluster(binary, topdir, workdir, initial_port):
    environment = ArangodEnvironment(binary, topdir, workdir, initial_port=initial_port)
    environment.start_agent()
    environment.start_coordinator()

    for i in range(2):
        environment.start_dbserver()

    return environment


def wait_for_cluster(client):
    while True:
        time.sleep(1)
        try:
            client.db('_system', username='root', verify=True)
            return True
        except ServerConnectionError:
            time.sleep(1)


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
            return (desired_workdir)


def run_test_arangosearch(client):
    # create a database to run the test on
    sys_db = client.db('_system', username='root', verify=True)
    sys_db.create_database('test')

    test_db = client.db('test', username='root')

    # collection into which documents will be inserted and indexed
    test_collection = test_db.create_collection('test_collection', shard_count=20)
    index = test_collection.add_inverted_index(fields=["field1"])
    test_db.create_view(name="test_view", view_type="arangosearch",
                        properties={"links": {
                            "test_collection": {
                                "includeAllFields": True}}})

    test_collection2 = test_db.create_collection('test_collection2')
    test_db.create_view(name="test_view_squared", view_type="arangosearch",
                        properties={"links": {
                            "test_collection2": {
                                "includeAllFields": True}}})

    # Start a transaction
    txn_db = test_db.begin_transaction(read="test_collection2", write="test_collection2")
    txn_col = txn_db.collection("test_collection2")
    txn_col.insert({"foo": "bar"})

    # Insert documents into test_collection
    stop_ev = threading.Event()
    insjob = threading.Thread(target=inserter, args=[test_collection, stop_ev])
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
    stop_ev.set()
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
    assert (p == [])
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


def run_test_aql(client):
    # create a database to run the test on
    sys_db = client.db('_system', username='root', verify=True)
    sys_db.create_database('test')

    test_db = client.db('test', username='root')

    # collection into which documents will be inserted and indexed
    test_collection = test_db.create_collection('test_collection', shard_count=20)

    stop_ev = threading.Event()

    def run_aql_thread(idx):
        while not stop_ev.is_set():
            test_db.aql.execute("FOR i IN 1..1000 INSERT {thrd: @idx, i} INTO test_collection",
                                  bind_vars={"idx": str(idx)})

    threads = []
    for i in range(10):
        thrd = threading.Thread(target=run_aql_thread, args=[i])
        thrd.start()
        threads.append(thrd)

    time.sleep(1)

    # Concurrently, take a hotbackup
    result = sys_db.backup.create(
        label="hotbackuptesthotbackup",
        allow_inconsistent=False,
        force=False,
        timeout=1000
    )
    print(f"backup_id: {result['backup_id']}")

    stop_ev.set()
    [t.join() for t in threads]

    # restore the hotbackup taken above
    sys_db.backup.restore(result["backup_id"])

    for i in range(10):
        result = list(test_db.aql.execute("""
            FOR doc IN test_collection
                FILTER doc.thrd == @idx
                COLLECT WITH COUNT INTO length
                RETURN length
        """, bind_vars={"idx": str(i)}))[0]

        # each thread writes batches of 1000 documents
        # thus is each query is transactional, we should only see multiples
        assert result % 1000 == 0, f"found {result} documents for thread {i}"



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

    tests = [run_test_arangosearch, run_test_aql]

    for idx, test in enumerate(tests):
        print(f"running test {test.__name__}")
        workdir = ensure_workdir(os.path.join(args.workdir, test.__name__))
        print(f"working directory is {workdir}")

        # give each env a different port range because of lingering
        environment = start_cluster(args.arangod, args.topdir, workdir, 8500 + idx * 100)

        client = ArangoClient(environment.get_coordinator_http_endpoint())
        wait_for_cluster(client)
        print(f"cluster ready")
        test(client)

        environment.shutdown()


if __name__ == "__main__":
    main()
