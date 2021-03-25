#!/usr/bin/python3

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

import os, re, sys
from typing import NamedTuple, List
from enum import Enum

# Check that we are in the right place:
lshere = os.listdir(".")
if not("arangod" in lshere and "arangosh" in lshere and \
        "Documentation" in lshere and "CMakeLists.txt" in lshere):
  print("Please execute me in the main source dir!")
  sys.exit(1)

# List files in Documentation/Metrics:
yamlfiles = os.listdir("Documentation/Metrics")
yamlfiles.sort()
if "allMetrics.yaml" in yamlfiles:
    yamlfiles.remove("allMetrics.yaml")
if "template.yaml" in yamlfiles:
    yamlfiles.remove("template.yaml")

class MetricType(Enum):
    gauge = 1
    counter = 2
    histogram = 3
    def fromStr(typeStr: str):
        return {
            "GAUGE": MetricType.gauge,
            "COUNTER": MetricType.counter,
            "HISTOGRAM": MetricType.histogram,
        }[typeStr]

class Metric(NamedTuple):
    name: str
    type: MetricType

# Read list of metrics from source:
metricsList: List[Metric] = []
headermatch = re.compile(r"DECLARE_(?P<type>COUNTER|GAUGE|HISTOGRAM)\s*\(")
namematch = re.compile(r"^\s*(?P<name>[a-z_A-Z0-9]+)\s*,")
for root, dirs, files in os.walk("."):
    if root[:10] == "./arangod/" or root[:6] == "./lib/":
      for f in files:
        if f[-4:] == ".cpp":
          ff = os.path.join(root, f)
          continuation = False
          s = open(ff, 'rt', encoding='utf-8')
          type : MetricType = None
          while True:
              l = s.readline()
              if l == "":
                  break
              if not(continuation):
                  m = headermatch.search(l)
                  if m:
                      type = MetricType.fromStr(m.group('type'))
                      continuation = True
                      l = l[m.end():]
              if continuation:
                  m = namematch.search(l)
                  if m:
                      name = m.group('name')
                      metricsList.append(Metric(name=name, type=type))
                      continuation = False
          if continuation:
              raise Exception("Unexpected EOF while parsing metric")
          s.close()
if len(metricsList) == 0:
    print("Did not find any metrics in arangod/RestServer/MetricsFeature.h!")
    sys.exit(2)

metricsList.sort()

# Check that every listed metric has a .yaml documentation file:
missing = False
yamls = []
for i in range(0, len(metricsList)):
    bad = False
    metricName = metricsList[i].name
    if not metricName + ".yaml" in yamlfiles:
        print("Missing metric documentation for metric '" + metricName + "'")
        bad = True
    else:
        # Check yaml:
        filename = os.path.join("Documentation/Metrics", metricName) + ".yaml"
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

metricNames = { metric.name : True for metric in metricsList }
tooMany = False
for i in range(0, len(yamlfiles)):
    name = yamlfiles[i][:-5]
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
