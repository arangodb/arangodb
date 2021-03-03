#!/usr/bin/python3

from yaml import load, dump, YAMLError
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

import os, re, sys

# Check that we are in the right place:
lshere = os.listdir(".")
if not("arangod" in lshere and "arangosh" in lshere and \
        "Documentation" in lshere and "CMakeLists.txt" in lshere):
  print("Please execute me in the main source dir!")
  sys.exit(1)

# List files in Documentation/Metrics:
yamlfiles = os.listdir("Documentation/Metrics")
yamlfiles.sort()

# Read list of metrics from source:
metricsList = []
linematch = re.compile("DECLARE_METRIC\(([a-z_A-Z]*)\)")
for root, dirs, files in os.walk("."):
    if root[:10] == "./arangod/" or root[:6] == "./lib/":
      for f in files:
        if f[-4:] == ".cpp":
          ff = os.path.join(root, f)
          s = open(ff)
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

metricsList.sort()

# Check that every listed metric has a .yaml documentation file:
missing = False
yamls = []
for i in range(0, len(metricsList)):
    bad = False
    if not metricsList[i] + ".yaml" in yamlfiles:
        print("Missing metric documentation for metric '" + metricsList[i] + "'")
        bad = True
    else:
        # Check yaml:
        filename = os.path.join("Documentation/Metrics", metricsList[i]) + ".yaml"
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
    
onlyCheck = False
if len(sys.argv) > 1 and sys.argv[1] == "-c":
  onlyCheck = True

outfile = "Documentation/Metrics/allMetrics.yaml"
output = dump(yamls, Dumper=Dumper)
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
