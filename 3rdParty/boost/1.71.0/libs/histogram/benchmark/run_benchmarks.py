#!/usr/bin/env python3
"""
Run this from a special build directory that uses the benchmark folder as root

    cd my_build_dir
    cmake ../benchmark
    ../run_benchmarks.py

This creates a database, benchmark_results. Plot it:

    ../plot_benchmarks.py

The script leaves the include folder in a modified state. To clean up, do:

    git checkout HEAD -- ../include
    git clean -f -- ../include

"""
import subprocess as subp
import tempfile
import os
import shelve
import json
import argparse


def get_commits():
    commits = []
    comments = {}
    for line in subp.check_output(("git", "log", "--oneline")).decode("ascii").split("\n"):
        if line:
            ispace = line.index(" ")
            hash = line[:ispace]
            commits.append(hash)
            comments[hash] = line[ispace+1:]
    commits = commits[::-1]
    return commits, comments


def recursion(results, commits, comments, ia, ib):
    ic = int((ia + ib) / 2)
    if ic == ia:
        return
    run(results, comments, commits[ic], False)
    if all([results[commits[i]] is None for i in (ia, ib, ic)]):
        return
    recursion(results, commits, comments, ic, ib)
    recursion(results, commits, comments, ia, ic)


def run(results, comments, hash, update):
    if not update and hash in results:
        return
    print(hash, comments[hash])
    subp.call(("rm", "-rf", "../include"))
    if subp.call(("git", "checkout", hash, "--", "../include")) != 0:
        print("[Benchmark] Cannot checkout include folder\n")
        return
    print(hash, "make")
    with tempfile.TemporaryFile() as out:
        if subp.call(("make", "-j4", "histogram_filling"), stdout=out, stderr=out) != 0:
            print("[Benchmark] Cannot make benchmarks\n")
            out.seek(0)
            print(out.read().decode("utf-8") + "\n")
            return
    print(hash, "run")
    s = subp.check_output(("./histogram_filling", "--benchmark_format=json", "--benchmark_filter=normal"))
    d = json.loads(s)
    if update and hash in results and results[hash] is not None:
        d2 = results[hash]
        for i, (b, b2) in enumerate(zip(d["benchmarks"], d2["benchmarks"])):
            d["benchmarks"][i] = b if b["cpu_time"] < b2["cpu_time"] else b2
    results[hash] = d
    for benchmark in d["benchmarks"]:
        print(benchmark["name"], min(benchmark["real_time"], benchmark["cpu_time"]))


def main():
    commits, comments = get_commits()

    parser = argparse.ArgumentParser()
    parser.add_argument("first", type=str, nargs="?", default=commits[0])
    parser.add_argument("last", type=str, nargs="?", default=commits[-1])
    parser.add_argument("-f", action="store_true")

    args = parser.parse_args()

    with shelve.open("benchmark_results") as results:
        a = commits.index(args.first)
        b = commits.index(args.last)
        if args.f:
            for hash in commits[a:b+1]:
                del results[hash]
        run(results, comments, args.first, False)
        run(results, comments, args.last, False)
        recursion(results, commits, comments, a, b)

if __name__ == "__main__":
    main()
