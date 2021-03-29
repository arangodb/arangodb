#!/usr/bin/python3

"""A script to check if all server metrics have documentation snippets."""

import os
import re
import sys

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

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
CATEGORYNAMES = ["Health", "AQL", "Transactions", "Foxx", "Pregel", \
                 "Statistics", "Replication", "Disk", "Errors", \
                 "RocksDB", "Hotbackup", "k8s", "Connectivity", "Network",\
                 "V8", "Agency", "Scheduler", "Maintenance", "kubearangodb"]

# Check that we are in the right place:
LS_HERE = os.listdir(".")
if not("arangod" in LS_HERE and "arangosh" in LS_HERE and \
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
LINEMATCHCOUNTER = re.compile(r"DECLARE_COUNTER\s*\(\s*([a-z_A-Z0-9]+)\s*,")
LINEMATCHGAUGE = re.compile(r"DECLARE_GAUGE\s*\(\s*([a-z_A-Z0-9]+)\s*,")
LINEMATCHHISTOGRAM = re.compile(r"DECLARE_HISTOGRAM\s*\(\s*([a-z_A-Z0-9]+)\s*,")
HEADERCOUNTER = re.compile(r"DECLARE_COUNTER\s*\(")
HEADERGAUGE = re.compile(r"DECLARE_GAUGE\s*\(")
HEADERHISTOGRAM = re.compile(r"DECLARE_HISTOGRAM\s*\(")
NAMEMATCH = re.compile(r"^\s*([a-z_A-Z0-9]+)\s*,")
for root, dirs, files in os.walk("."):
    if root[:10] == "./arangod/" or root[:6] == "./lib/":
        for f in files:
            if f[-4:] == ".cpp":
                ff = os.path.join(root, f)
                continuation = False
                s = open(ff, 'rt', encoding='utf-8')
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
                s.close()
if len(METRICSLIST) == 0:
    print("Did not find any metrics in arangod/RestServer/MetricsFeature.h!")
    sys.exit(2)

METRICSLIST.sort()

# Check that every listed metric has a .yaml documentation file:
MISSING = False
YAMLS = []
for i, metric in enumerate(METRICSLIST):
    bad = False
    if not metric + ".yaml" in YAMLFILES:
        print("Missing metric documentation for metric '" + metric + "'")
        bad = True
    else:
        # Check yaml:
        filename = os.path.join("Documentation/Metrics", metric) + ".yaml"
        try:
            s = open(filename)
        except FileNotFoundError:
            print("Could not open file '" + filename + "'")
            bad = True
            continue
        try:
            y = load(s, Loader=Loader)
        except YAMLError as err:
            print("Could not parse YAML file '" + filename + "', error:\n" + str(err))
            bad = True
            continue

        YAMLS.append(y)   # for later dump

        # Check a few things in the yaml:
        for attr in ["name", "help", "exposedBy", "description", "unit",\
                     "type", "category", "complexity", "introducedIn"]:
            if not attr in y:
                print("YAML file '" + filename + \
                      "' does not have required attribute '" + attr + "'")
                bad = True
        if not bad:
            for attr in ["name", "help", "description", "unit",\
                         "type", "category", "complexity", "introducedIn"]:
                if not isinstance(y[attr], str):
                    print("YAML file '" + filename + "' has an attribute '" +\
                          attr + "' whose value must be a string but isn't.")
                    bad = True
            if not isinstance(y["exposedBy"], list):
                print("YAML file '" + filename + \
                      "' has an attribute 'exposedBy' whose value " + \
                      "must be a list but isn't.")
                bad = True
            if not bad:
                if not y["category"] in CATEGORYNAMES:
                    print("YAML file '" + filename + \
                          "' has an unknown category '" + y["category"] + \
                          "', please fix.")
                    bad = True

    if bad:
        MISSING = True

TOOMANY = False
for i, yamlfile in enumerate(YAMLFILES):
    name = yamlfile[:-5]
    if not name in METRICSLIST:
        TOOMANY = True
        print("YAML file '" + name + \
              ".yaml'\n  does not have a corresponding metric " + \
              "declared in the source code!")

DUMPMETRICS = False
if len(sys.argv) > 1 and sys.argv[1] == "-d":
    DUMPMETRICS = True

OUTFILE = "Documentation/Metrics/allMetrics.yaml"
OUTPUT = dump(YAMLS, Dumper=Dumper)
if DUMPMETRICS:
    # Dump what we have:
    S = open(OUTFILE, "w")
    S.write(OUTPUT)
    S.close()

if MISSING:
    sys.exit(17)

if TOOMANY:
    sys.exit(18)
