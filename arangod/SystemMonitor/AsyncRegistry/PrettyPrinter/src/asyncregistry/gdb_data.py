from __future__ import annotations
from typing import Iterable
from dataclasses import dataclass
import gdb

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
            return "\"" + self.name[len("arangodb::async_registry::State::"):] + "\""
        else:
            return self.name
    
@dataclass
class Thread:
    posix_id: gdb.Value
    lwpid: gdb.Value

    @classmethod
    def from_gdb(cls, value: gdb.Value | None):
        if not value:
            return None
        return cls(value['posix_id'], value['kernel_id'])

    def __str__(self):
        return f"LWPID {self.lwpid} (pthread {self.posix_id})"

@dataclass
class ThreadInfo:
    lwpid: gdb.Value
    name: gdb.Value

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['kernel_id'], value['name'])

    def __str__(self):
        return f"{self.name} (LWPID {self.lwpid})"

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
    content: ThreadInfo | PromiseId

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        num_flag_bits = 1
        flag_mask = (1 << num_flag_bits) - 1
        BIT_WIDTH_64 = 64
        all_ones_64_bit_mask = (1 << BIT_WIDTH_64) - 1
        data_mask_64bit = flag_mask ^ all_ones_64_bit_mask
        if value & flag_mask:
            return cls(ThreadInfo.from_gdb(
                (value & data_mask_64bit)
                .cast(gdb.lookup_type("arangodb::containers::SharedResource<arangodb::basics::ThreadInfo>").pointer())
                .dereference()['_data']
            ))
        else:
            return cls(PromiseId(
                (value & data_mask_64bit)
                .cast(gdb.lookup_type("void").pointer())
            ))

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
    owning_thread: ThreadInfo
    running_thread: Optional[Thread]
    source_location: SourceLocation
    requester: Requester
    state: State

    @classmethod
    def from_gdb(cls, ptr: gdb.Value, value: gdb.Value):
        return cls(
            PromiseId(ptr),
            ThreadInfo.from_gdb(value["owning_thread"]["_resource"].dereference()["_data"]),
            Thread.from_gdb(GdbOptional.from_gdb(value["running_thread"])._value),
            SourceLocation.from_gdb(value["source_location"]),
            Requester.from_gdb(value["requester"]["_resource"]["_M_i"]),
            State.from_gdb(value["state"])
        )

    def is_valid(self):
        return not self.state.is_deleted()

    def __str__(self):
        thread_str = f" on {self.running_thread}" if self.running_thread else ""
        return str(self.source_location) + ", owned by " + str(self.owning_thread) + ", " + str(self.state) + thread_str

@dataclass
class GdbOptional:
    _value: Optional[gdb.Value]

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        payload = value["_M_i"]["_M_payload"]
        engaged = payload["_M_engaged"]
        if not engaged:
            return cls(None)
        internal_value = payload["_M_payload"]["_M_value"]
        return cls(internal_value)
    
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
        return cls(Promise.from_gdb(node_ptr, node_ptr.dereference()["data"]) for node_ptr in GdbAtomicList(value["_head"]["_M_b"]["_M_p"]))

class GdbVector:
    def __init__(self, value: gdb.Value):
        self._begin = value["_M_start"]
        self._end = value["_M_finish"]
        
    def __iter__(self):
        current = self._begin
        while current != self._end:
            registry = current.dereference()["_M_ptr"]
            current += 1
            yield registry

@dataclass
class AsyncRegistry:
    thread_registries: Iterable[ThreadRegistry]

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(ThreadRegistry.from_gdb(registry) for registry in GdbVector(value['_lists']['_M_impl']))

    def promises(self) -> Iterable[Promise]:
        for registry in self.thread_registries:
            for promise in registry.promises:
                yield promise
