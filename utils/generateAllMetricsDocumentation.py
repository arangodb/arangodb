#!/usr/bin/python3

"""A script to check if all server metrics have documentation snippets."""

import os
import re
import sys
from pathlib import Path
from itertools import chain

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader
except ImportError:
    from yaml import Loader

# Usage:

def print_usage():
    """Print usage."""
    print("""Usage: utils/generateAllMetricsDocumentation.py [-h] [--help] [-d]

    -d:    if this option is given, a combined file with all metrics
           documentation snippets is written to 
              Documentation/Metrics/allMetrics.yaml
           otherwise, this script only checks the format of all snippets and
           that there is one exactly one metrics declarations for each snippet.

    use -h or --help for this help.
""")

if len(sys.argv) > 1 and (sys.argv[1] == "-h" or sys.argv[1] == "--help"):
    print_usage()
    sys.exit(0)

# Some data:
CATEGORYNAMES = ["Health", "AQL", "Transactions", "Foxx", "Pregel",
                 "Statistics", "Replication", "Disk", "Errors",
                 "RocksDB", "Hotbackup", "k8s", "Connectivity", "Network",
                 "V8", "Agency", "Scheduler", "Maintenance", "kubearangodb",
                 "License", "ArangoSearch"]

NODE_TYPES = ["coordinator", "dbserver", "agent", "single"]

# Check that we are in the right place:
LS_HERE = os.listdir(".")
if not("arangod" in LS_HERE and "client-tools" in LS_HERE and
       "Documentation" in LS_HERE and "CMakeLists.txt" in LS_HERE):
    print("Please execute me in the main source dir!")
    sys.exit(1)

# List files in Documentation/Metrics:
YAMLFILES = os.listdir("Documentation/Metrics")
YAMLFILES.sort()
if "allMetrics.yaml" in YAMLFILES:
    YAMLFILES.remove("allMetrics.yaml")
if "template.yaml" in YAMLFILES:
    YAMLFILES.remove("template.yaml")

# Read list of metrics from source:
METRICSLIST = []
LINEMATCHCOUNTER = re.compile(r"^\s*DECLARE_COUNTER\s*\(\s*([a-z_A-Z0-9]+)\s*,")
LINEMATCHGAUGE = re.compile(r"^\s*DECLARE_GAUGE\s*\(\s*([a-z_A-Z0-9]+)\s*,")
LINEMATCHHISTOGRAM = re.compile(r"^\s*DECLARE_HISTOGRAM\s*\(\s*([a-z_A-Z0-9]+)\s*,")
HEADERCOUNTER = re.compile(r"^\s*DECLARE_COUNTER\s*\(")
HEADERGAUGE = re.compile(r"^\s*DECLARE_GAUGE\s*\(")
HEADERHISTOGRAM = re.compile(r"^\s*DECLARE_HISTOGRAM\s*\(")
NAMEMATCH = re.compile(r"^\s*([a-z_A-Z0-9]+)\s*,")

files = chain(*[ Path(rootdir).glob(pattern)
            for rootdir in ["arangod", "lib", "enterprise"]
            for pattern in ["**/*.cpp", "**/*.h"] ])

for f in files:
    continuation = False
    with open(f, encoding="utf-8") as s:
        while True:
            l = s.readline()
            if not continuation:
                if l == "":
                    break
                m = LINEMATCHCOUNTER.search(l)
                if m:
                    METRICSLIST.append(m.group(1))
                    continue
                m = LINEMATCHGAUGE.search(l)
                if m:
                    METRICSLIST.append(m.group(1))
                    continue
                m = LINEMATCHHISTOGRAM.search(l)
                if m:
                    METRICSLIST.append(m.group(1))
                    continue
                m = HEADERCOUNTER.search(l)
                if m:
                    continuation = True
                m = HEADERGAUGE.search(l)
                if m:
                    continuation = True
                m = HEADERHISTOGRAM.search(l)
                if m:
                    continuation = True
            else:
                continuation = False
                m = NAMEMATCH.search(l)
                if m:
                    METRICSLIST.append(m.group(1))
if len(METRICSLIST) == 0:
    print("Did not find any metrics in arangod/Metrics/MetricsFeature.h!")
    sys.exit(2)

METRICSLIST.sort()

# Check that every listed metric has a .yaml documentation file:
MISSING = False
YAMLS = []
for i, metric in enumerate(METRICSLIST):
    bad = False
    if not metric + ".yaml" in YAMLFILES:
        print(f"Missing metric documentation for metric '{metric}'")
        bad = True
    else:
        # Check yaml:
        filename = os.path.join("Documentation", "Metrics", metric) + ".yaml"
        try:
            s = open(filename, encoding="utf-8")
        except FileNotFoundError:
            print(f"Could not open file '{filename}'")
            bad = True
        if not bad:
            try:
                y = load(s, Loader=Loader)
            except YAMLError as err:
                print(f"Could not parse YAML file '{filename}', error:\n{err}")
                bad = True
            finally:
                s.close()

        if not bad:
            YAMLS.append(y)   # for later dump

            # Check a few things in the yaml:
            for attr in ["name", "help", "exposedBy", "description", "unit",
                         "type", "category", "complexity", "introducedIn"]:
                if not attr in y:
                    print(f"YAML file '{filename}' does not have required attribute '{attr}'")
                    bad = True
            if not bad:
                for attr in ["name", "help", "description", "unit",
                             "type", "category", "complexity", "introducedIn"]:
                    if not isinstance(y[attr], str):
                        print(f"YAML file '{filename}' has an attribute "
                              f"'{attr}' whose value must be a string but isn't.")
                        bad = True
                if y["help"].strip()[-1] != ".":
                    print(f"YAML file '{filename}' has a 'help' attribute that "
                          f"does not end with a period.")
                    bad = True
                if not isinstance(y["exposedBy"], list):
                    print(f"YAML file '{filename}' has an attribute 'exposedBy' "
                          f"whose value must be a list but isn't.")
                    bad = True
                else:
                    for e in y["exposedBy"]:
                        if e not in NODE_TYPES:
                            print(f"YAML file '{filename}' has an attribute 'exposedBy' "
                                  f"that lists an invalid value '{e}'.")
                            bad = True
                if not bad:
                    if not y["category"] in CATEGORYNAMES:
                        print(f"YAML file '{filename}' has an unknown category "
                              f"'{y['category']}', please fix.")
                        bad = True
                    if not bad:
                        if y["name"] != metric:
                            print(f"YAML file '{filename}' has an attribute name '" + y["name"] + "' which does not match the file name.")
                            bad = True

    if bad:
        MISSING = True

TOOMANY = False
for i, yamlfile in enumerate(YAMLFILES):
    name = yamlfile[:-5]
    if not name in METRICSLIST:
        TOOMANY = True
        print(f"YAML file '{name}.yaml'\n"
              f"does not have a corresponding metric declared in the source code!")

DUMPMETRICS = False
if len(sys.argv) > 1 and sys.argv[1] == "-d":
    DUMPMETRICS = True

OUTFILE = "Documentation/Metrics/allMetrics.yaml"
if DUMPMETRICS:
    # Dump what we have:
    with open(OUTFILE, "w", encoding="utf-8") as S:
        for i, yamlfile in enumerate(YAMLFILES):
            with open(os.path.join("Documentation", "Metrics", yamlfile), encoding="utf-8") as I:
                prefix = "- "
                while True:
                    l = I.readline()
                    if l == "":
                        break
                    S.write(prefix)
                    prefix = "  "
                    S.write(l)
                S.write("\n")

if MISSING:
    sys.exit(17)

if TOOMANY:
    sys.exit(18)
