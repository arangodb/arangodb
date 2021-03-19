#!/usr/bin/python3

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

import os, re, sys, json

# Usage:

def printUsage():
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
lshere = os.listdir(".")
if not("arangod" in lshere and "arangosh" in lshere and \
        "Documentation" in lshere and "CMakeLists.txt" in lshere):
  print("Please execute me in the main source dir!", file=sys.stderr)
  sys.exit(1)

yamlfile = "Documentation/Metrics/allMetrics.yaml"

# Database about categories and personas and their interests:

categoryNames = ["Health", "AQL", "Transactions", "Foxx", "Pregel", \
                 "Statistics", "Replication", "Disk", "Errors", \
                 "RocksDB", "Hotbackup", "k8s", "Connectivity", "Network",\
                 "V8", "Agency", "Scheduler", "Maintenance", "kubearangodb"]

complexities = ["none", "simple", "medium", "advanced"]

personainterests = {}
personainterests["all"] = \
    { "Health":       "advanced", \
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
personainterests["dbadmin"] = \
    { "Health":       "advanced", \
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
personainterests["sysadmin"] = \
    { "Health":       "advanced", \
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
personainterests["user"] = \
    { "Health":       "simple", \
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
personainterests["oasiscustomer"] = \
    { "Health":       "simple", \
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
personainterests["appdeveloper"] = \
    { "Health":       "medium", \
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
personainterests["oasisoncall"] = \
    { "Health":       "advanced", \
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
personainterests["arangodbdeveloper"] = \
    { "Health":       "advanced", \
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

persona = "all"
i = 1
while i < len(sys.argv):
    if sys.argv[i] == "-h" or sys.argv[i] == "--help":
        printUsage()
        sys.exit(0)
    if sys.argv[i] == "-p":
        if i+1 < len(sys.argv):
            p = sys.argv[i+1]
            if p in personainterests:
                persona = p
            else:
                print("Warning: persona " + p + " not known.", file=sys.stderr)
            i += 2
        else:
            i += 1

# Now read all metrics from the `allMetrics.yaml` file:

try:
    allMetricsFile = open(yamlfile)
except FileNotFoundError:
    print("Could not open file '" + yamlfile + "'!", file=sys.stderr)
    sys.exit(1)

try:
    allMetrics= load(allMetricsFile, Loader=Loader)
except YAMLError as err:
    print("Could not parse YAML file '" + yamlfile + "', error:\n" + str(err), file=sys.stderr)
    sys.exit(2)

allMetricsFile.close()

# And generate the output:

categories = {}
metrics = {}
for m in allMetrics:
    c = m["category"]
    if not c in categoryNames:
        print("Warning: Found category", c, "which is not in our current list!", file=sys.stderr)
    if not c in categories:
        categories[c] = []
    categories[c].append(m["name"])
    metrics[m["name"]] = m

posx = 0
posy = 0

out = { "panels" : [] }
panels = out["panels"]

def incxy(x, y):
    x += 12
    if x >= 24:
        x = 0
        y += 8
    return x, y

def makePanel(x, y, met):
    return {"gridPos": {"h": 8, "w": 12, "x": x, "y": y }, \
            "description": met["description"], \
            "title": met["help"]}

for c in categoryNames:
    if c in categories:
        ca = categories[c]
        if len(ca) > 0:
            # Find complexity interest of persona:
            complexitylimit = complexities.index(personainterests[persona][c])
            # Make row:
            row = {"gridPos": {"h":1, "w": 24, "x": 0, "y": posy}, \
                   "panels": [], "title": c, "type": "row"}
            posx = 0
            posy += 1
            panels.append(row)
            for name in ca:
              met = metrics[name]
              if complexities.index(met["complexity"]) <= complexitylimit:
                panel = makePanel(posx, posy, met)
                if met["type"] == "counter":
                    panel["type"] = "graph"
                    panel["targets"] = [ { "expr": "rate(" + met["name"] + \
                                                   "[1m])", \
                         "legendFormat": "{{instance}}:{{shortname}}" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                elif met["type"] == "gauge":
                    panel["type"] = "graph"
                    panel["targets"] = [ { "expr": met["name"], \
                         "legendFormat": "{{instance}}:{{shortname}}" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                elif met["type"] == "histogram":
                    panel["type"] = "heatmap"
                    panel["legend"] = {"show": False}
                    panel["targets"] = [ \
                        { "expr": "histogram_quantile(0.95, sum(rate(" + \
                          met["name"] + "_bucket[60s])) by (le))", \
                          "format": "heatmap", \
                          "legendFormat": "" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                    panel = makePanel(posx, posy, met)
                    panel["title"] = panel["title"] + " (count of events per second)"
                    panel["type"] = "graph"
                    panel["targets"] = [ \
                        { "expr": "rate(" + met["name"] + "_count[60s])", \
                          "legendFormat": "{{instance}}:{{shortname}}" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                    panel = makePanel(posx, posy, met)
                    panel["title"] = panel["title"] + " (average per second)"
                    panel["type"] = "graph"
                    panel["targets"] = [ \
                        { "expr": "rate(" + met["name"] + "_sum[60s])" + \
                                  " / rate(" + met["name"] + "_count[60s])", \
                          "legendFormat": "{{instance}}:{{shortname}}" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                else:
                    print("Strange metrics type:", met["type"], file=sys.stderr)

json.dump(out, sys.stdout)


