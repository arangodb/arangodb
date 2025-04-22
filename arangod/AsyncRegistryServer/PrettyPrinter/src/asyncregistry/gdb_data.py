from __future__ import annotations
from typing import Iterable
from dataclasses import dataclass

@dataclass
class State:
    name: str

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(str(value['_M_i']))

    def is_deleted(self):
        return self.name == "arangodb::async_registry::State::Deleted"

    def __str__(self):
        if len(self.name) > len("arangodb::async_registry::State::"):
            return self.name[len("arangodb::async_registry::State::"):]
        else:
            return self.name
    
@dataclass
class Thread:
    posix_id: gdb.Value
    lwpid: gdb.Value
    # TODO is there a way to get the thread name?

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['posix_id'], value['kernel_id'])

    def __str__(self):
        return "thread " + str(self.lwpid)

@dataclass
class SourceLocation:
    file_name: gdb.Value
    function_name: gdb.Value
    line: gdb.Value

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['file_name'], value['function_name'], value['line']['_M_i'])

    def __str__(self):
        return str(self.function_name) + " (" +  str(self.file_name) + ":" + str(self.line) + ")"

@dataclass
class Requester:
    content: Thread | PromiseId

    @classmethod
    def from_gdb(cls, value: gdb.Value):
       alternative = value['_M_index']
       if alternative == 0:
           content = Thread.from_gdb(value['_M_u']['_M_first']['_M_storage'])
       elif alternative == 1:
           content = PromiseId(value['_M_u']['_M_rest']['_M_first']['_M_storage'])
       else:
           return "wrong input"
       return cls(content)

    def __str__(self):
        return str(self.content)

@dataclass
class PromiseId:
    id: gdb.Value
    
    def __init__(self, id):
        self.id = id

    def __equ__(self, other: PromiseId) -> bool:
       return self.id == other.id 

    def __str__(self):
        return str(self.id)

@dataclass
class Promise:
    id: PromiseId
    thread: Thread
    source_location: SourceLocation
    requester: Requester
    state: State

    @classmethod
    def from_gdb(cls, value_ptr: gdb.Value):
        value = value_ptr.dereference()
        return cls(
            PromiseId(value_ptr),
            Thread.from_gdb(value["thread"]),
            SourceLocation.from_gdb(value["source_location"]),
            Requester.from_gdb(value["requester"]['_M_i']),
            State.from_gdb(value["state"])
        )

    def is_valid(self):
        return not self.state.is_deleted()

    def __str__(self):
        return str(self.source_location) + ", " + str(self.thread) + ", " + str(self.state)

@dataclass
class GdbAtomicList:
    _head_ptr: gdb.Value

    def __iter__(self):
        current = self._head_ptr
        next = current
        while next != 0:
            current = next
            next = current.dereference()["next"]
            yield current

@dataclass
class ThreadRegistry:
    promises: Iterable[Promise]

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(Promise.from_gdb(promise) for promise in GdbAtomicList(value["promise_head"]["_M_b"]["_M_p"]))

class GdbVector:
    def __init__(self, value: gdb.Value):
        self._begin = value["_M_start"]
        self._end = value["_M_finish"]
        
    def __iter__(self):
        current = self._begin
        while current != self._end:
            registry = current.dereference()["_M_ptr"].dereference()
            current += 1
            yield registry

@dataclass
class AsyncRegistry:
    thread_registries: Iterable[ThreadRegistry]

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(ThreadRegistry.from_gdb(registry) for registry in GdbVector(value['registries']['_M_impl']))

    def promises(self) -> Iterable[Promise]:
        for registry in self.thread_registries:
            for promise in registry.promises:
                yield promise
