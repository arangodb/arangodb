#!/usr/bin/env python3

import sys
import json
from typing import List

class Thread(object):
    def __init__(self, name: str, id: int):
        self.name = name
        self.id = id
    def __str__(self):
        return self.name + "(" + str(self.id) + ")"

class SourceLocation(object):
    def __init__(self, file_name: str, line: int, function_name: str):
        self.file_name = file_name
        self.line = line
        self.function_name = function_name
    def __str__(self):
        return self.function_name + " (" + self.file_name + ":" + str(self.line) + ")"
    
class Data(object):
    def __init__(self, owning_thread: Thread, source_location: SourceLocation, id: int, waiter: int, state: str):
        self.owning_thread = owning_thread
        self.source_location = source_location
        self.id = id
        self.waiter = waiter
        self.state = state
    def __str__(self):
        return str(SourceLocation(**self.source_location)) + ", " + str(Thread(**self.owning_thread)) + ", " + self.state
        
class Promise(object):
    def __init__(self, hierarchy: int, data: Data):
        self.hierarchy = hierarchy
        self.data = data
    def __str__(self):
        return ''.join(["  " for x in range(self.hierarchy)]) + str(Data(**self.data))
    
class Stacktrace(object):
    def __init__(self, promises: List[Promise]):
        self.promises = promises
    def __str__(self):
        return "\n".join(str(Promise(**x)) for x in self.promises)
        
def test_intput() -> str:
    return """{"promise_stacktraces": [
  [
    {
      "hierarchy": 0,
      "data": {
        "owning_thread": {
          "name": "AgencyCache",
          "id": "124252722300608"
        },
        "source_location": {
          "file_name": "/home/jvolmer/code/arangodb/arangod/Network/Methods.cpp",
          "line": 242,
          "function_name": "arangodb::network::(anonymous namespace)::Pack::Pack(DestinationId &&, RequestLane, bool, bool)"
        },
        "id": 124252709790688,
        "waiter": 0,
        "state": "Suspended"
      }
    }
  ],
  [
    {
      "hierarchy": 3,
      "data": {
        "owning_thread": {
          "name": "AgencyCache",
          "id": "124252722300608"
        },
        "source_location": {
          "file_name": "/home/jvolmer/code/arangodb/arangod/Network/Methods.cpp",
          "line": 242,
          "function_name": "arangodb::network::(anonymous namespace)::Pack::Pack(DestinationId &&, RequestLane, bool, bool)"
        },
        "id": 124252709790688,
        "waiter": 124252709790368,
        "state": "Suspended"
      }
    },
    {
      "hierarchy": 2,
      "data": {
        "owning_thread": {
          "name": "AgencyCache",
          "id": "124252722300608"
        },
        "source_location": {
          "file_name": "/home/jvolmer/code/arangodb/arangod/Network/Methods.cpp",
          "line": 367,
          "function_name": "FutureRes arangodb::network::sendRequest(ConnectionPool *, DestinationId, RestVerb, std::string, velocypack::Buffer<uint8_t>, const RequestOptions &, Headers)"
        },
        "id": 124252709790368,
        "waiter": 124252709790848,
        "state": "Suspended"
      }
    },
    {
      "hierarchy": 1,
      "data": {
        "owning_thread": {
          "name": "AgencyCache",
          "id": "124252722300608"
        },
        "source_location": {
          "file_name": "/home/jvolmer/code/arangodb/arangod/Agency/AsyncAgencyComm.cpp",
          "line": 325,
          "function_name": "auto (anonymous namespace)::agencyAsyncSend(AsyncAgencyCommManager &, RequestMeta &&, VPackBuffer<uint8_t> &&)::(anonymous class)::operator()(auto) [auto:1 = arangodb::futures::Unit]"
        },
        "id": 124252709790848,
        "waiter": 124252709791008,
        "state": "Suspended"
      }
    },
    {
      "hierarchy": 0,
      "data": {
        "owning_thread": {
          "name": "AgencyCache",
          "id": "124252722300608"
        },
        "source_location": {
          "file_name": "/home/jvolmer/code/arangodb/arangod/Agency/AsyncAgencyComm.cpp",
          "line": 325,
          "function_name": "auto (anonymous namespace)::agencyAsyncSend(AsyncAgencyCommManager &, RequestMeta &&, VPackBuffer<uint8_t> &&)::(anonymous class)::operator()(auto) [auto:1 = arangodb::futures::Unit]"
        },
        "id": 124252709791008,
        "waiter": 0,
        "state": "Suspended"
      }
    }
  ]
    ]}"""

def main():
    # string = test_intput()
    string = sys.stdin.read()
    data = json.loads(string)["promise_stacktraces"]
    print("\n\n".join(str(Stacktrace(**{"promises": x})) for x in data))
    

if __name__ == '__main__':
    main()
