#!/usr/bin/env python3

"""Task Monitoring Pretty Printer

This script pretty-prints the hierarchical task monitoring JSON output from ArangoDB.
Groups by top-level task and state, and displays as ASCII trees.

Usage: cat <monitoring_output.json> | ./pretty_printer.py [--show-deleted]
"""

import sys
import json
import argparse
from taskmonitoring.tasktree import TaskTree

def main():
    parser = argparse.ArgumentParser(description="Pretty print ArangoDB task monitoring output.")
    parser.add_argument('--show-deleted', action='store_true', help='Show Deleted tasks (default: hide)')
    args = parser.parse_args()

    string = sys.stdin.read()
    data = json.loads(string)["task_stacktraces"]
    tree = TaskTree.from_json(data)
    tree.pretty_print(show_deleted=args.show_deleted)

if __name__ == '__main__':
    main() 