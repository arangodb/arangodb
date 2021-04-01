#!/usr/bin/python3

import os
import re
import sys
from typing import NamedTuple, List, Optional
from enum import Enum

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

# Check that we are in the right place:
LS_HERE = os.listdir(".")
if not("arangod" in LS_HERE and "arangosh" in LS_HERE and
        "Documentation" in LS_HERE and "CMakeLists.txt" in LS_HERE):
  print("Please execute me in the main source dir!")
  sys.exit(1)

# List files in Documentation/Metrics:
YAMLFILES = [f for f in os.listdir("Documentation/Metrics")
        if not f.startswith('.')]
YAMLFILES.sort()
if "allMetrics.yaml" in YAMLFILES:
    YAMLFILES.remove("allMetrics.yaml")
if "template.yaml" in YAMLFILES:
    YAMLFILES.remove("template.yaml")

class MetricType(Enum):
    gauge = 1
    counter = 2
    histogram = 3
    def fromStr(typeStr: str):
        return {
            "gauge": MetricType.gauge,
            "counter": MetricType.counter,
            "histogram": MetricType.histogram,
        }[typeStr.lower()]

class Metric(NamedTuple):
    name: str
    type: MetricType
    file: str

# Read list of metrics from source:
METRICSLIST: List[Metric] = []
HEADERMATCH = re.compile(r"DECLARE_(?P<type>COUNTER|GAUGE|HISTOGRAM)\s*\(")
NAMEMATCH = re.compile(r"^\s*(?P<name>[a-z_A-Z0-9]+)\s*,")
for root, dirs, files in os.walk("."):
    if root[:10] == "./arangod/" or root[:6] == "./lib/":
        for f in files:
            if f[-4:] == ".cpp":
                ff = os.path.join(root, f)
                continuation = False
                s = open(ff, 'rt', encoding='utf-8')
                type : Optional[MetricType] = None
                while True:
                    l = s.readline()
                    if l == "":
                        break
                    if not(continuation):
                        m = HEADERMATCH.search(l)
                        if m:
                            type = MetricType.fromStr(m.group('type'))
                            continuation = True
                            l = l[m.end():]
                    if continuation:
                        m = NAMEMATCH.search(l)
                        if m:
                            name = m.group('name')
                            METRICSLIST.append(Metric(name=name, type=type, file=f))
                            continuation = False
                if continuation:
                    raise Exception("Unexpected EOF while parsing metric")
                s.close()
if len(METRICSLIST) == 0:
    print("Did not find any metrics in arangod/RestServer/MetricsFeature.h!")
    sys.exit(2)

METRICSLIST.sort()

def verifyYaml(metric, y, fileName, filePath):
    # Check a few things in the yaml:
    for attr in ["name", "help", "exposedBy", "description", "unit",
                 "type", "category", "complexity"]:
        if not attr in y:
            print("YAML file '" + filePath + "' does not have required attribute '" + attr + "'")
            return False
    for attr in ["name", "help", "description", "unit",
                 "type", "category", "complexity"]:
        if not isinstance(y[attr], str):
            print("YAML file '" + filePath + "' has an attribute '" + attr + "' whose value must be a string but isn't.")
            return False
    if not isinstance(y["exposedBy"], list):
        print("YAML file '" + filePath + "' has an attribute 'exposedBy' whose value must be a list but isn't.")
        return False
    if y['name'] != metric.name:
        print("Metric name '{}' defined in cpp file '{}' "
                "doesn't match name '{}' defined in YAML file '{}'"
                .format(metric.name, metric.file, y['name'], fileName))
        return False
    knownTypes = ['gauge', 'counter', 'histogram']
    if not y['type'] in knownTypes:
        print("Unexpected type '{}', expected one of {} in YAML file '{}'"
                .format(y['type'], knownTypes, fileName))
        return False
    elif MetricType.fromStr(y['type']) != metric.type:
        print("Type mismatch for metric '{}': "
                "Defined as {} in cpp file '{}', "
                "but defined as {} in YAML file '{}'."
                .format(metric.name, metric.type.name, metric.file,
                    y['type'], fileName))
        return False
    return True

# Check that every listed metric has a .yaml documentation file:
missing = False
yamls = []
for i in range(0, len(METRICSLIST)):
    bad = False
    metric = METRICSLIST[i]
    fileName = metric.name + ".yaml"
    if not fileName in YAMLFILES:
        print("Missing metric documentation for metric '" + metric.name + "'")
        bad = True
    else:
        # Check yaml:
        filePath = os.path.join("Documentation/Metrics", fileName)
        try:
            s = open(filePath)
        except FileNotFoundError:
            print("Could not open file '" + filePath + "'")
            bad = True
            continue
        try:
            y = load(s, Loader=Loader)
        except YAMLError as err:
            print("Could not parse YAML file '" + filePath + "', error:\n" + str(err))
            bad = True
            continue

        yamls.append(y)   # for later dump

        if not verifyYaml(metric, y, fileName, filePath):
            bad = True


    if bad:
        missing = True

metricNames = { metric.name : True for metric in METRICSLIST }
tooMany = False
for i in range(0, len(YAMLFILES)):
    name = YAMLFILES[i][:-5]
    if not(name in metricNames):
        tooMany = True
        print("YAML file '" + name + ".yaml'\n  does not have a corresponding metric declared in the source code!")

onlyCheck = False
if len(sys.argv) > 1 and sys.argv[1] == "-c":
    onlyCheck = True

outfile = "Documentation/Metrics/allMetrics.yaml"
output = dump(yamls, Dumper=Dumper, default_flow_style=False)
if onlyCheck:
    input = open(outfile, "r")
    found = input.read()
    input.close()
    if output != found:
        print("""
Generated file Documentation/Metrics/allMetrics.yaml does not match
the metrics documentation snippets. Please run

  utils/generateAllMetricsDocumentation.py

and commit Documentation/Metrics/allMetrics.yaml with your PR!
""")
    sys.exit(18)
else:
    # Dump what we have:
    s = open(outfile, "w")
    s.write(output)
    s.close()

if missing:
    sys.exit(17)

if tooMany:
    sys.exit(18)
