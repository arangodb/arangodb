#!/usr/bin/python3

"""A script to automatically make dashboards for Grafana."""

import os
import sys
import json

from yaml import load, YAMLError
try:
    from yaml import CLoader as Loader
except ImportError:
    from yaml import Loader

# Usage:

def print_usage():
    """Print usage."""
    print("""Usage: utils/makeDashboards.py [-p PERSONA] [-h] [--help]

    where PERSONA is one of:
      - "dbadmin": Database administrator
      - "sysadmin": System administrator
      - "user": Pure user of ArangoDB
      - "oasiscustomer": Customer of Oasis
      - "appdeveloper": Application developer
      - "oasisoncall": Oasis on call personell
      - "arangodbdeveloper": ArangoDB developer
      - "all": Produce all metrics (default)

    use -h or --help for this help.
""")

# Check that we are in the right place:
LS_HERE = os.listdir(".")
if not("arangod" in LS_HERE and "arangosh" in LS_HERE and \
        "Documentation" in LS_HERE and "CMakeLists.txt" in LS_HERE):
    print("Please execute me in the main source dir!", file=sys.stderr)
    sys.exit(1)

YAMLFILE = "Documentation/Metrics/allMetrics.yaml"

# Database about categories and personas and their interests:

CATEGORYNAMES = ["Health", "AQL", "Transactions", "Foxx", "Pregel", \
                 "Statistics", "Replication", "Disk", "Errors", \
                 "RocksDB", "Hotbackup", "k8s", "Connectivity", "Network",\
                 "V8", "Agency", "Scheduler", "Maintenance", "kubearangodb"]

COMPLEXITIES = ["none", "simple", "medium", "advanced"]

PERSONAINTERESTS = {}
PERSONAINTERESTS["all"] = \
    {"Health":       "advanced", \
     "AQL":          "advanced", \
     "Transactions": "advanced", \
     "Foxx":         "advanced", \
     "Pregel":       "advanced", \
     "Statistics":   "advanced", \
     "Replication":  "advanced", \
     "Disk":         "advanced", \
     "Errors":       "advanced", \
     "RocksDB":      "advanced", \
     "Hotbackup":    "advanced", \
     "k8s":          "advanced", \
     "Connectivity": "advanced", \
     "Network":      "advanced", \
     "V8":           "advanced", \
     "Agency":       "advanced", \
     "Scheduler":    "advanced", \
     "Maintenance":  "advanced", \
     "kubearangodb": "advanced", \
    }
PERSONAINTERESTS["dbadmin"] = \
    {"Health":       "advanced", \
     "AQL":          "advanced", \
     "Transactions": "simple", \
     "Foxx":         "simple", \
     "Pregel":       "simple", \
     "Statistics":   "medium", \
     "Replication":  "advanced", \
     "Disk":         "advanced", \
     "Errors":       "simple", \
     "RocksDB":      "simple", \
     "Hotbackup":    "simple", \
     "k8s":          "medium", \
     "Connectivity": "simple", \
     "Network":      "medium", \
     "V8":           "medium", \
     "Agency":       "none", \
     "Scheduler":    "none", \
     "Maintenance":  "none", \
     "kubearangodb": "medium", \
    }
PERSONAINTERESTS["sysadmin"] = \
    {"Health":       "advanced", \
     "AQL":          "simple", \
     "Transactions": "simple", \
     "Foxx":         "simple", \
     "Pregel":       "simple", \
     "Statistics":   "medium", \
     "Replication":  "advanced", \
     "Disk":         "advanced", \
     "Errors":       "simple", \
     "RocksDB":      "none", \
     "Hotbackup":    "none", \
     "k8s":          "medium", \
     "Connectivity": "simple", \
     "Network":      "medium", \
     "V8":           "medium", \
     "Agency":       "none", \
     "Scheduler":    "none", \
     "Maintenance":  "none", \
     "kubearangodb": "simple", \
    }
PERSONAINTERESTS["user"] = \
    {"Health":       "simple", \
     "AQL":          "medium", \
     "Transactions": "advanced", \
     "Foxx":         "medium", \
     "Pregel":       "medium", \
     "Statistics":   "simple", \
     "Replication":  "simple", \
     "Disk":         "simple", \
     "Errors":       "medium", \
     "RocksDB":      "none", \
     "Hotbackup":    "none", \
     "k8s":          "medium", \
     "Connectivity": "simple", \
     "Network":      "simple", \
     "V8":           "medium", \
     "Agency":       "none", \
     "Scheduler":    "none", \
     "Maintenance":  "none", \
     "kubearangodb": "none", \
    }
PERSONAINTERESTS["oasiscustomer"] = \
    {"Health":       "simple", \
     "AQL":          "medium", \
     "Transactions": "advanced", \
     "Foxx":         "medium", \
     "Pregel":       "medium", \
     "Statistics":   "simple", \
     "Replication":  "medium", \
     "Disk":         "simple", \
     "Errors":       "medium", \
     "RocksDB":      "none", \
     "Hotbackup":    "simple", \
     "k8s":          "none", \
     "Connectivity": "none", \
     "Network":      "simple", \
     "V8":           "medium", \
     "Agency":       "none", \
     "Scheduler":    "none", \
     "Maintenance":  "none", \
     "kubearangodb": "none", \
    }
PERSONAINTERESTS["appdeveloper"] = \
    {"Health":       "medium", \
     "AQL":          "advanced", \
     "Transactions": "advanced", \
     "Foxx":         "advanced", \
     "Pregel":       "advanced", \
     "Statistics":   "medium", \
     "Replication":  "simple", \
     "Disk":         "simple", \
     "Errors":       "advanced", \
     "RocksDB":      "simple", \
     "Hotbackup":    "none", \
     "k8s":          "simple", \
     "Connectivity": "none", \
     "Network":      "medium", \
     "V8":           "advanced", \
     "Agency":       "none", \
     "Scheduler":    "simple", \
     "Maintenance":  "none", \
     "kubearangodb": "none", \
    }
PERSONAINTERESTS["oasisoncall"] = \
    {"Health":       "advanced", \
     "AQL":          "medium", \
     "Transactions": "medium", \
     "Foxx":         "medium", \
     "Pregel":       "medium", \
     "Statistics":   "medium", \
     "Replication":  "medium", \
     "Disk":         "advanced", \
     "Errors":       "medium", \
     "RocksDB":      "medium", \
     "Hotbackup":    "medium", \
     "k8s":          "medium", \
     "Connectivity": "medium", \
     "Network":      "medium", \
     "V8":           "medium", \
     "Agency":       "simple", \
     "Scheduler":    "medium", \
     "Maintenance":  "simple", \
     "kubearangodb": "medium", \
    }
PERSONAINTERESTS["arangodbdeveloper"] = \
    {"Health":       "advanced", \
     "AQL":          "advanced", \
     "Transactions": "advanced", \
     "Foxx":         "advanced", \
     "Pregel":       "advanced", \
     "Statistics":   "advanced", \
     "Replication":  "advanced", \
     "Disk":         "advanced", \
     "Errors":       "advanced", \
     "RocksDB":      "advanced", \
     "Hotbackup":    "advanced", \
     "k8s":          "advanced", \
     "Connectivity": "advanced", \
     "Network":      "advanced", \
     "V8":           "advanced", \
     "Agency":       "advanced", \
     "Scheduler":    "advanced", \
     "Maintenance":  "advanced", \
     "kubearangodb": "advanced", \
    }

# Parse command line options:

PERSONA = "all"
i = 1
while i < len(sys.argv):
    if sys.argv[i] == "-h" or sys.argv[i] == "--help":
        print_usage()
        sys.exit(0)
    if sys.argv[i] == "-p":
        if i+1 < len(sys.argv):
            P = sys.argv[i+1]
            if P in PERSONAINTERESTS:
                PERSONA = P
            else:
                print("Warning: persona " + P + " not known.", file=sys.stderr)
                sys.exit(1)
            i += 2
        else:
            i += 1

# Now read all metrics from the `allMetrics.yaml` file:

try:
    ALLMETRICSFILE = open(YAMLFILE)
except FileNotFoundError:
    print("Could not open file '" + YAMLFILE + "'!", file=sys.stderr)
    sys.exit(1)

try:
    ALLMETRICS = load(ALLMETRICSFILE, Loader=Loader)
except YAMLError as err:
    print("Could not parse YAML file '" + YAMLFILE + "', error:\n" + str(err), file=sys.stderr)
    sys.exit(2)

ALLMETRICSFILE.close()

# And generate the output:

CATEGORIES = {}
METRICS = {}
for m in ALLMETRICS:
    c = m["category"]
    if not c in CATEGORYNAMES:
        print("Warning: Found category", c, "which is not in our current list!", file=sys.stderr)
    if not c in CATEGORIES:
        CATEGORIES[c] = []
    CATEGORIES[c].append(m["name"])
    METRICS[m["name"]] = m

POSX = 0
POSY = 0

OUT = {"panels" : []}
PANELS = OUT["panels"]

def incxy(x, y):
    """Increment coordinates."""
    x += 12
    if x >= 24:
        x = 0
        y += 8
    return x, y

def make_panel(x, y, mett):
    """Make a panel."""
    title = mett["help"]
    while title[-1:] == "." or title[-1:] == "\n":
        title = title[:-1]
    return {"gridPos": {"h": 8, "w": 12, "x": x, "y": y}, \
            "description": mett["description"], \
            "title": title}

for c in CATEGORYNAMES:
    if c in CATEGORIES:
        ca = CATEGORIES[c]
        if len(ca) > 0:
            # Find complexity interest of persona:
            complexitylimit = COMPLEXITIES.index(PERSONAINTERESTS[PERSONA][c])
            # Make row:
            row = {"gridPos": {"h":1, "w": 24, "x": 0, "y": POSY}, \
                   "panels": [], "title": c, "type": "row"}
            POSX = 0
            POSY += 1
            PANELS.append(row)
            for name in ca:
                met = METRICS[name]
                if COMPLEXITIES.index(met["complexity"]) <= complexitylimit:
                    panel = make_panel(POSX, POSY, met)
                    if met["type"] == "counter":
                        panel["type"] = "graph"
                        panel["targets"] = [{"expr": "rate(" + met["name"] + \
                                                     "[1m])", \
                            "legendFormat": "{{instance}}:{{shortname}}"}]
                        POSX, POSY = incxy(POSX, POSY)
                        PANELS.append(panel)
                    elif met["type"] == "gauge":
                        panel["type"] = "graph"
                        panel["targets"] = [{"expr": met["name"], \
                            "legendFormat": "{{instance}}:{{shortname}}"}]
                        POSX, POSY = incxy(POSX, POSY)
                        PANELS.append(panel)
                    elif met["type"] == "histogram":
                        panel["type"] = "heatmap"
                        panel["legend"] = {"show": False}
                        panel["targets"] = [\
                            {"expr": "histogram_quantile(0.95, sum(rate(" + \
                             met["name"] + "_bucket[60s])) by (le))", \
                             "format": "heatmap", \
                             "legendFormat": ""}]
                        POSX, POSY = incxy(POSX, POSY)
                        PANELS.append(panel)
                        panel = make_panel(POSX, POSY, met)
                        panel["title"] = panel["title"] + " (count of events per second)"
                        panel["type"] = "graph"
                        panel["targets"] = [\
                            {"expr": "rate(" + met["name"] + "_count[60s])", \
                             "legendFormat": "{{instance}}:{{shortname}}"}]
                        POSX, POSY = incxy(POSX, POSY)
                        PANELS.append(panel)
                        panel = make_panel(POSX, POSY, met)
                        panel["title"] = panel["title"] + " (average per second)"
                        panel["type"] = "graph"
                        panel["targets"] = [\
                            {"expr": "rate(" + met["name"] + "_sum[60s])" + \
                                      " / rate(" + met["name"] + "_count[60s])", \
                              "legendFormat": "{{instance}}:{{shortname}}"}]
                        POSX, POSY = incxy(POSX, POSY)
                        PANELS.append(panel)
                    else:
                        print("Strange metrics type:", met["type"], file=sys.stderr)

json.dump(OUT, sys.stdout)
