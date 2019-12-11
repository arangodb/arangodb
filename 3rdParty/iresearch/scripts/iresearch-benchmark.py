#!/usr/bin/env python
import platform
import os
import subprocess
import shutil
import sys

CWD = os.path.dirname(os.path.abspath(__file__))
DEPS_DIR = os.path.join(CWD, "../iresearch.deps")
BENCHMARK_RESOURCES_ROOT = os.path.join(DEPS_DIR, "benchmark_resources")
TEST_RESOURCE_ROOT = os.path.join(DEPS_DIR, "test_resources/tests/resources")
host_system = platform.system();
use_time_utility = host_system == "Linux"


def main():
  if host_system == "Linux":
    os.environ["LD_LIBRARY_PATH"] = os.environ["LD_LIBRARY_PATH"] + ":"  + os.path.join(CWD, "bin")
  
  


runs = range(1, 2)
lineSeq = [1, 5, 10, 15, 20, 25]

def runIndex(withLucene):
  for i in runs:
    for j in lineSeq :
      MAX_LINES = j * 1000000
      shutil.rmtree("iresearch.data");

def runSearch(withLucene):

def runBenchmarkedProcess(process, arguments):
	if use_time_utility:
		


if __name__== "__main__":
  main()