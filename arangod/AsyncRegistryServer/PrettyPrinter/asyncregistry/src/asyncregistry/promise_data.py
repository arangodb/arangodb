from __future__ import annotations
from typing import Iterable

class State:
    def __init__(self, name: str):
        self.name = name

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(str(value['_M_i']))

    def is_deleted(self):
        return self.name == "arangodb::async_registry::State::Deleted"

    def __str__(self):
        return self.name
    
class Thread:
    def __init__(self, posix_id, lwpid):
        # TODO is there a way to get the thread name?
        self.posix_id = posix_id
        self.lwpid = lwpid

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['posix_id'], value['kernel_id'])

    def __str__(self):
        return "thread LWPID: " + str(self.lwpid) + " & posix_id: " + str(self.posix_id)

class Requester:
    def __init__(self, alternative, content: Thread | PromiseId):
        self.alternative = alternative
        self.content = content

    @classmethod
    def from_gdb(cls, value: gdb.Value):
       alternative = value['_M_index']
       if alternative == 0:
           content = Thread.from_gdb(value['_M_u']['_M_first']['_M_storage'])
       elif alternative == 1:
           content = PromiseId(value['_M_u']['_M_rest']['_M_first']['_M_storage'])
       else:
           return "wrong input"
       return cls(alternative, content)

    def __str__(self):
        return str(self.content)

class SourceLocation:
    def __init__(self, file_name, function_name, line):
        self.file_name = file_name
        self.function_name = function_name
        self.line = line

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['file_name'], value['function_name'], value['line']['_M_i'])

    def __str__(self):
        return str(self.file_name) + ":" + str(self.line) + " " + str(self.function_name)

class PromiseId:
    def __init__(self, id):
        self.id = id

    def __str__(self):
        return str(self.id)

class Promise:
    # raw print in gdb: print /r *(registry.registries[0]._M_ptr).promise_head._M_b._M_p
    
    def __init__(self, id: PromiseId, thread: Thread, source_location: SourceLocation, requester: Requester, state: State):
        self.id = id
        self.thread = thread
        self.source_location = source_location
        self.requester = requester
        self.state = state

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
        return str(self.id) + ": waiter " + str(self.requester) + " " + str(self.source_location) + ", " + str(self.thread) + ", " + str(self.state) 

class GdbAtomicList:
    def __init__(self, begin):
        self._head_ptr = begin

    def __iter__(self):
        current = self._head_ptr
        next = current
        while next != 0:
            current = next
            next = current.dereference()["next"]
            yield current

class ThreadRegistry:
    def __init__(self, promises: Iterable[MyTest]):
       self.promises = promises

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(Promise.from_gdb(promise) for promise in GdbAtomicList(value["promise_head"]["_M_b"]["_M_p"]))

    def __str__(self):
        return "\n".join(str(promise) for promise in filter(lambda x: x.is_valid(), self.promises))
    
# special gdb vector
class GdbVector:
    def __init__(self, value):
        self._begin = value["_M_start"]
        self._end = value["_M_finish"]
        
    def __iter__(self):
        current = self._begin
        while current != self._end:
            registry = current.dereference()["_M_ptr"].dereference()
            current += 1
            yield registry

class AsyncRegistry:
    def __init__(self, registries: Iterable[ThreadRegistry]):
        self.thread_registries = registries

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(ThreadRegistry.from_gdb(registry) for registry in GdbVector(value['registries']['_M_impl']))

    def promises(self) -> Iterable[MyTest]:
        for registry in self.thread_registries:
            for promise in registry.promises:
                yield promise
