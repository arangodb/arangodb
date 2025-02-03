import argparse
import datetime
import math
import time

import h5py
import sys
import os
import multiprocessing as mp
import requests
import logging


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
)

# Test Script for the vector index
# Uses hdf5 datasets from this repository
# https://github.com/erikbern/ann-benchmarks
#
# Example usage:
#   python3 scripts/hdf5-vector-index-test.py gist-960-euclidean.hdf5 --auth.basic root: \
#   --endpoint http://localhost:8529 --number-of-shards 3 --index.metric l2 --index.factory IVF3800_HNSW32,PQ480x8 \
#   --index.nlists 3800  --drop-collection --nprobe 1 2 4 8 16 32 64 128 256


def import_vectors(endpoints, auth, collection, database, data, number_of_workers=32):
    total_size = len(data)

    logging.info(f"Importing {total_size} vectors")

    def check_status(response):
        if response.status_code != 202:
            raise RuntimeError(f"Writing documents failed: status={response.status_code}, response={response.text}")

    def write_parts(worked_id):
        ep = endpoints[worked_id % len(endpoints)]
        url = os.path.join(ep, f"_db/{database}/_api/document/{collection}?silent=true")

        docs = []
        number_of_docs_per_worker = total_size // number_of_workers
        i = worked_id * number_of_docs_per_worker
        for value in data[worked_id * number_of_docs_per_worker:(worked_id + 1) * number_of_docs_per_worker]:
            docs.append({"v": value.tolist(), "i": i})
            i += 1

            if len(docs) == 1000:
                response = requests.post(url, json=docs, auth=auth)
                check_status(response)
                docs = []

        if len(docs) > 0:
            response = requests.post(url, json=docs, auth=auth)
            check_status(response)

    threads = list()

    start = datetime.datetime.now()

    for k in range(number_of_workers):
        t = mp.Process(target=write_parts, args=[k])
        t.start()
        threads.append(t)

    logging.info(f"Started {number_of_workers} import workers.")
    for t in threads:
        t.join()
        if t.exitcode != 0:
            raise RuntimeError("Child process failed")

    duration = datetime.datetime.now() - start
    logging.info(f"Import done - took {duration}")


def parse_hdf5_file(file):
    h = h5py.File(file)

    train = h["train"]
    test = h["test"]
    neighbors = h["neighbors"]
    distances = h["distances"]

    return train, test, neighbors, distances


def get_auth_token(arguments):
    jwt = arguments.get("auth.jwt-token")
    if jwt is not None:
        return jwt
    user_pass = arguments.get("auth.basic")
    if user_pass is not None:
        return tuple(user_pass.split(':', 1))
    return None


def check_auth(endpoints, auth):
    for ep in endpoints:
        try:
            url = os.path.join(ep, "_api/version")
            response = requests.get(url, auth=auth)
            if response.status_code != 200:
                raise RuntimeError(
                    f"Server {ep} refused api version call, response: {response.status_code} {response.text}")
        except Exception as e:
            raise RuntimeError(f"failed to connect to {ep}")


def parse_arguments():
    args = argparse.ArgumentParser()
    args.add_argument("--collection", default="v", type=str)
    args.add_argument("--drop-collection", help="Drop collection is already exist", default=False, action='store_true')
    args.add_argument("--number-of-shards", default=3, type=int)
    args.add_argument("--database", default="_system", type=str)
    args.add_argument("input", type=str)
    args.add_argument("--number-of-threads", default=24, type=int)
    args.add_argument("--test-only", default=False, action='store_true')
    args.add_argument("--endpoint", default=["http://localhost:8529/"], nargs='+')
    args.add_argument("--nprobe", default=[1], nargs='+')

    args.add_argument("--index.nlists", default=None)
    args.add_argument("--index.metric", choices=("l2", "cosine"), default="l2")
    args.add_argument("--index.factory", required=False, type=str)

    auth_group = args.add_mutually_exclusive_group()
    auth_group.add_argument("--auth.jwt-token", type=str)
    auth_group.add_argument("--auth.basic", type=str)

    return vars(args.parse_args())


def drop_collection(ep, auth, collection, database):
    response = requests.delete(os.path.join(ep, f"_db/{database}/_api/collection/{collection}"), auth=auth)

    if response.status_code not in [200, 404]:
        raise RuntimeError(f"failed to drop collection: {response.text}")
    if response.status_code == 200:
        logging.info("Dropped existing collection")


def create_collection(ep, auth, collection, database, number_of_shards):
    response = requests.post(os.path.join(ep, f"_db/{database}/_api/collection"), auth=auth, json={
        "name": collection, "numberOfShards": number_of_shards
    })

    if response.status_code != 200:
        raise RuntimeError(f"failed to create collection: {response.text}")
    logging.info("Collection created")


def ensure_index(ep, auth, collection, database, metric, nlists, dim, factory):
    assert nlists is not None

    index_definition = {
        "fields": ["v"],
        "type": "vector",
        "inBackground": True,
        "params": {
            "metric": metric,
            "nLists": int(nlists),
            "dimension": dim
        }
    }

    if factory is not None:
        index_definition["params"]["factory"] = factory

    logging.info(f"Start creating vector index, definition = {index_definition}")
    start = datetime.datetime.now()
    response = requests.post(os.path.join(ep, f"_db/{database}/_api/index?collection={collection}"), auth=auth,
                             json=index_definition)
    end = datetime.datetime.now()
    if response.status_code not in [200, 201]:
        raise RuntimeError(f"failed to create index: {response.text}")
    print(f"Index created - took {end - start}")


def compute_nlists_argument(args, num_vectors):
    if args["index.nlists"] is not None:
        return args["index.nlists"]

    vectors_per_shard = num_vectors // args["number_of_shards"]

    return math.ceil(math.sqrt(15 * vectors_per_shard))


def run_query(ep, auth, database, query, bindVars):
    query_url = os.path.join(ep, f"_db/{database}/_api/cursor")
    response = requests.post(query_url, auth=auth, json={"query": query, "bindVars": bindVars})

    if response.status_code != 201:
        raise RuntimeError(f"failed to create cursor: {response.text}")

    result = response.json()
    total_results = result["result"]
    has_more = result["hasMore"]

    # TODO implement has_more == True
    assert has_more == False
    return total_results


def run_single_test(ep, auth, collection, database, metric, test_vector, neighbors, distances, nprobe):
    dir = "DESC" if metric == "cosine" else "ASC"
    query = (f"FOR doc IN {collection} LET d = APPROX_NEAR_{metric.upper()}(@q, doc.v, {{nProbe: {nprobe}}}) "
             f"SORT d {dir} LIMIT {len(neighbors)} RETURN [doc.i, d]")
    start = time.perf_counter()
    result = run_query(ep, auth, database, query, {"q": test_vector.tolist()})
    total_time = time.perf_counter() - start

    found_neighbors = set((n for n, _ in result))
    actual_neighbors = set(neighbors)

    true_positives = found_neighbors.intersection(actual_neighbors)

    recall = len(true_positives) / len(neighbors)
    precision = len(true_positives) / len(result)
    return recall, precision, total_time


def run_test(ep, auth, collection, database, metric, test, neighbors, distances, nprobe):
    assert len(test) == len(neighbors) == len(distances)

    total_recall = 0
    total_precision = 0
    total_time = 0

    for v, n, d in zip(test, neighbors, distances):
        r, p, t = run_single_test(ep[0], auth, collection, database, metric, v, n, d, nprobe)
        total_recall += r
        total_precision += p
        total_time += t

    print(f"nprobe = {nprobe}, avg. time = {total_time / len(test)}s, avg. recall = {total_recall / len(test)},"
          f" avg. precision = {total_precision / len(test)}")


def run_tests(ep, auth, collection, database, metric, test, neighbors, distances, nprobe):
    for np in nprobe:
        run_test(ep, auth, collection, database, metric, test, neighbors, distances, np)


def real_main():
    args = parse_arguments()
    print(args)
    ep = args["endpoint"]

    auth = get_auth_token(args)
    check_auth(ep, auth)

    train, test, neighbors, distances = parse_hdf5_file(args["input"])

    dim = len(train[0])
    logging.info(f"Dataset dimension is {dim}")

    collection = args["collection"]
    database = args["database"]
    metric = args["index.metric"]
    nprobe = args["nprobe"]

    if not args["test_only"]:
        if args["drop_collection"]:
            drop_collection(ep[0], auth, collection, database)
        create_collection(ep[0], auth, collection, database, args["number_of_shards"])
        import_vectors(ep, auth, collection, database, train, args["number_of_threads"])

        nlists = compute_nlists_argument(args, len(train))

        ensure_index(ep[0], auth, collection, database, metric, nlists, dim, args["index.factory"])

    run_tests(ep, auth, collection, database, metric, test, neighbors, distances, nprobe)


if __name__ == "__main__":
    real_main()
