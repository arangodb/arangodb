#!/usr/bin/env python

import threading
import argparse
import tempfile
import time

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

def main():
    parser = argparse.ArgumentParser(
        prog="HotBackupConsistencyTest.py",
        description="tests hotbackups",
        epilog="")
    parser.add_argument('--arangod')
    parser.add_argument('--topdir')
    args = parser.parse_args()

    workdir = tempfile.mkdtemp(prefix="arangotest_")
    environment = ArangodEnvironment(args.arangod, args.topdir, workdir)

    agent = Agent(environment)
    agent.start()

    coordinator = Coordinator(environment)
    coordinator.start()

    for i in range(2):
        dbserver = DBServer(environment)
        dbserver.start()

    client = ArangoClient(hosts="http://localhost:8501")
    sys_db = client.db('_system', username='root')
    backup = sys_db.backup

    # TODO: replace this with API call for readiness
    time.sleep(15)

    sys_db.create_database('test')

    test_db = client.db('test', username='root')
    test_collection = test_db.create_collection('test_collection')
    test_collection2 = test_db.create_collection('test_collection2')

    index = test_collection.add_inverted_index(fields=["field1"])
    test_db.create_view(name="test_view", view_type="arangosearch",
                        properties = { "links": { "test_collection": { "includeAllFields": True }} })

    txn_db = test_db.begin_transaction(read="test_collection2", write="test_collection2")
    txn_col = txn_db.collection("test_collection2")
    txn_col.insert({"foo": "bar"})

    insjob = threading.Thread(target=inserter, args=[test_collection])
    insjob.start()

    time.sleep(1)

    print("Taking a hot backup...")
    result = backup.create(
        label="hotbackuptesthotbackup",
        allow_inconsistent=False,
        force=False,
        timeout=1000
    )
    print(f"result: {result}")
    insjob.join()

    print(f"Restoring hotbackup with id {result['backup_id']}")
    result = backup.restore(result["backup_id"])
    print(f"result: {result}")

    print("Checking database consistency")
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

    p = set(test_db.aql.execute("FOR v IN test_collection RETURN v._key"))
    q = set(test_db.aql.execute("""
        FOR w IN test_collection
             FILTER w.field1 == "stone"
             RETURN w._key
        """))
    assert p == q

if __name__ == "__main__":
    main()
