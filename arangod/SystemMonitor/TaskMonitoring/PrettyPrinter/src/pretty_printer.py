#!/usr/bin/env python3

"""Task Monitoring Pretty Printer

This script pretty-prints the hierarchical task monitoring JSON output from ArangoDB.
Groups by top-level task and state, and displays as ASCII trees.

Usage: cat <monitoring_output.json> | ./pretty_printer.py
"""

import sys
import json
from taskmonitoring.tasktree import TaskTree

def main():
    string = sys.stdin.read()
    data = json.loads(string)["task_stacktraces"]
    tree = TaskTree.from_json(data)
    tree.pretty_print()

if __name__ == '__main__':
    main() 