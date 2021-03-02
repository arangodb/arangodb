#!/usr/bin/python3

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

import os, re, sys

# List files in Documentation/Metrics:
files = os.listdir("Documentation/Metrics")
files.sort()

# Read list of metrics from source:
s = open("arangod/RestServer/MetricsFeature.h")
linematch = re.compile("inline +constexpr +char +([a-z_A-Z]*) *\[] *=")
metricsList = []
while True:
    l = s.readline()
    if l == "":
        break
    m = linematch.search(l)
    if m:
        metricsList.append(m.group(1))
s.close()
if len(metricsList) == 0:
    print("Did not find any metrics in arangod/RestServer/MetricsFeature.h!")
    sys.exit(2)

# Check that every listed metric has a .yaml documentation file:
missing = False
yamls = []
for i in range(0, len(metricsList)):
    bad = False
    if not metricsList[i] + ".yaml" in files:
        print("Missing metric documentation for metric '" + metricsList[i] + "'")
        bad = True
    else:
        # Check yaml:
        filename = "Documentation/Metrics/" + metricsList[i] + ".yaml"
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

        yamls.append(y)   # for later dump

        # Check a few things in the yaml:
        for attr in ["name", "help", "exposedBy", "description", "unit",\
                     "type", "category", "complexity"]:
            if not attr in y:
                print("YAML file '" + filename + "' does not have required attribute '" + attr + "'")
                bad = True
        if not bad:
            for attr in ["name", "help", "description", "unit",\
                         "type", "category", "complexity"]:
                if not isinstance(y[attr], str):
                    print("YAML file '" + filename + "' has an attribute '" + attr + "' whose value must be a string but isn't.")
                    bad = True
            if not isinstance(y["exposedBy"], list):
                print("YAML file '" + filename + "' has an attribute 'exposedBy' whose value must be a list but isn't.")
                bad = True
            
    if bad:
        missing = True
        
# Dump what we have:
output = dump(yamls, Dumper=Dumper)
s = open("Documentation/Metrics/allMetrics.yaml", "w")
s.write(output)
s.close()

if missing:
    sys.exit(17)
