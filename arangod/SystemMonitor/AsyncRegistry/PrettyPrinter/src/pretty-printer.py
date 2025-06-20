#!/usr/bin/env python3

"""Async Registry Pretty Printer

This script allows a user to print the async stacktraces from server_url/_admin/async-registry as a list of trees.

Usage: curl -s server_url/_admin/async-registry | ./pretty_printer.py

"""

import sys
import json
from typing import List
from typing import Optional
from asyncregistry.stacktrace import Stacktrace

class Thread(object):
    def __init__(self, id: int, posix_id: int):
        self.id = id
        self.posix_id = posix_id
    @classmethod
    def from_json(cls, blob: dict):
        return cls(blob["LWPID"], blob["posix_id"])
    def __str__(self):
        return f"LWPID {self.id} (pthread {self.posix_id})"

class ThreadInfo(object):
    def __init__(self, lwpid: int, name: str):
        self.lwpid = lwpid
        self.name = name
    @classmethod
    def from_json(cls, blob: dict):
        return cls(blob["LWPID"], blob["name"])
    def __str__(self):
        return f"{self.name} (LWPID {self.lwpid})"

class SourceLocation(object):
    def __init__(self, file_name: str, line: int, function_name: str):
        self.file_name = file_name
        self.line = line
        self.function_name = function_name
    @classmethod
    def from_json(cls, blob: dict):
        return cls(blob["file_name"], blob["line"], blob["function_name"])
    def __str__(self):
        return self.function_name + " (" + self.file_name + ":" + str(self.line) + ")"

class PromiseId(object):
    def __init__(self, id):
        self.id = id
    @classmethod
    def from_json(cls, blob: dict):
        return cls(blob["id"])
    def __str__(self):
        return self.id
    
class Requester(object):
    def __init__(self, is_sync: bool, item: ThreadInfo | PromiseId):
        self.is_sync = is_sync
        self.item = item
    @classmethod
    def from_json(cls, blob: dict):
        if not blob:
            return None
        if "LWPID" in blob:
            return cls(True, ThreadInfo.from_json(blob))
        else:
            return cls(False, PromiseId.from_json(blob))
    def __str__(self):
        if self.is_sync:
            # a sync requester is always at the bottom of a tree,
            # but has no entry on its own,
            # therefore just add it here in a new line
            return "\n" + "─ " + str(self.item)
        else:
            return ""

class Data(object):
    def __init__(self, owning_thread: ThreadInfo, running_thread: Optional[Thread], source_location: SourceLocation, id: int, state: str, requester: Requester):
        self.owning_thread = owning_thread
        self.running_thread = running_thread
        self.source_location = source_location
        self.id = id
        self.waiter = requester
        self.state = state
    @classmethod
    def from_json(cls, blob: dict):
        return cls(ThreadInfo.from_json(blob["owning_thread"]), Thread.from_json(blob["running_thread"]) if "running_thread" in blob else None, SourceLocation.from_json(blob["source_location"]), blob["id"], blob["state"], Requester.from_json(blob["requester"]))
    def __str__(self):
        waiter_str = str(self.waiter) if self.waiter != None else ""
        thread_str = f" on {self.running_thread}" if self.running_thread else ""
        return str(self.source_location) + ", owned by " + str(self.owning_thread) + ", " + self.state + thread_str + waiter_str
        
class Promise(object):
    def __init__(self, hierarchy: int, data: Data):
        self.hierarchy = hierarchy
        self.data = data
    
def main():
    string = sys.stdin.read()
    data = json.loads(string)["promise_stacktraces"]
    first = True
    for promise_list in data:
        current_stacktrace = Stacktrace()
        for promise_entry in promise_list:
            stacktrace = current_stacktrace.append(promise_entry["hierarchy"], promise_entry["data"])
            if stacktrace != None:
                if not first:
                    print("\n")
                print("\n".join(ascii + str(Data.from_json(promise)) for (ascii, promise) in stacktrace))
                first = False
                current_stacktrace = Stacktrace()

if __name__ == '__main__':
    main()
