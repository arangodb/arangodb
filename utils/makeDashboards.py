#!/usr/bin/python3

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

import os, re, sys, json

# Check that we are in the right place:
lshere = os.listdir(".")
if not("arangod" in lshere and "arangosh" in lshere and \
        "Documentation" in lshere and "CMakeLists.txt" in lshere):
  print("Please execute me in the main source dir!")
  sys.exit(1)

yamlfile = "Documentation/Metrics/allMetrics.yaml"

try:
    allMetricsFile = open(yamlfile)
except FileNotFoundError:
    print("Could not open file '" + yamlfile + "'!")
    sys.exit(1)

try:
    allMetrics= load(allMetricsFile, Loader=Loader)
except YAMLError as err:
    print("Could not parse YAML file '" + yamlfile + "', error:\n" + str(err))
    sys.exit(2)

allMetricsFile.close()

categoryNames = ["Health", "AQL", "Transactions", "Foxx", "Pregel", \
                 "Statistics", "Replication", "Disk", "Errors", \
                 "RocksDB", "Hotbackup", "k8s", "Connectivity", "Network",\
                 "V8", "Agency", "Scheduler", "Maintenance", "kubearangodb"]

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
            # Make row:
            row = {"gridPos": {"h":1, "w": 24, "x": 0, "y": posy}, \
                   "panels": [], "title": c, "type": "row"}
            posx = 0
            posy += 1
            panels.append(row)
            for name in ca:
                met = metrics[name]
                panel = makePanel(posx, posy, met)
                if met["type"] == "counter" or \
                   met["type"] == "gauge":
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
                          "legendFormat": "{{instance}}:{{shortname}" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                    panel = makePanel(posx, posy, met)
                    panel["title"] = panel["title"] + " (average per second)"
                    panel["type"] = "graph"
                    panel["targets"] = [ \
                        { "expr": "rate(" + met["name"] + "_sum[60s])" + \
                                  " / rate(" + met["name"] + "_count[60s])", \
                          "legendFormat": "{{instance}}:{{shortname}" } ]
                    posx, posy = incxy(posx, posy)
                    panels.append(panel)
                else:
                    print("Strange metrics type:", met["type"], file=sys.stderr)

json.dump(out, sys.stdout)





