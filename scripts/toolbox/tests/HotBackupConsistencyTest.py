#!/usr/bin/env python

import threading
from termcolor import colored
from arango import ArangoClient
from modules import ArangoDBInstanceManager
from modules import ArangodEnvironment

def inserter(collection):
    print("hello from inserter\n")
    for i in range(10000):
        collection.insert({"number": i, "field1": "stone"})
    print("done inserting")

def main():
    parser = argparse.ArgumentParser(
        prog="hotbackup-test.py",
        description="tests hotbackups",
        epilog="")
    parser.add_argument('--arangod')
    args = parser.parse_args()

    workdir = tempfile.mkdtemp(prefix="arangotest")
    print(f"workdir: {workdir}")
    environment = ArangodEnvironment(args.arangod, workdir)

    print("starting agent")
    agent = Agent(environment)
    agent.start()

    print("starting coordinator")
    coordinator = Coordinator(environment)
    coordinator.start()

    print("starting dbserver")
    for i in range(2):
        dbserver = DBServer(environment)
        dbserver.start()
    print("done")

    client = ArangoClient(hosts="http://localhost:8501")
    sys_db = client.db('_system', username='root')
    backup = sys_db.backup

    # YOLO
    time.sleep(15)

    print("create test db")
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

    print("hotbackup lol")
    result = backup.create(
        label="hotbackuptesthotbackup",
        allow_inconsistent=False,
        force=False,
        timeout=1000
    )
    print(f"result: {result}")
    insjob.join()

    print("hotbackup restore lol")
    result = backup.restore(result["backup_id"])
    print(f"lolol {result}")
    print("hotrestore done")

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
