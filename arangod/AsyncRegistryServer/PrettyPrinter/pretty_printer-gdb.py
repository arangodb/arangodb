"""Async Registry Pretty Printer for gdb

Parsing 

"""
from typing import Iterable

class State:
    def __init__(self, name: str):
        self._name = name

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(str(value['_M_i']))

    def is_deleted(self):
        return self._name == "arangodb::async_registry::State::Deleted"

    def __str__(self):
        return self._name
    
class Thread:
    def __init__(self, posix_id, lwpid):
        # TODO is there a way to get the thread name?
        self._posix_id = posix_id
        self._lwpid = lwpid

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['posix_id'], value['kernel_id'])

    def __str__(self):
        return "thread LWPID: " + str(self._lwpid) + " & posix_id: " + str(self._posix_id)

class Requester:
    def __init__(self, alternative, content: Thread | PromiseId):
        self._alternative = alternative
        self._content = content

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
        return str(self._content)

class SourceLocation:
    def __init__(self, file_name, function_name, line):
        self._file_name = file_name
        self._function_name = function_name
        self._line = line

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(value['file_name'], value['function_name'], value['line']['_M_i'])

    def __str__(self):
        return str(self._file_name) + ":" + str(self._line) + " " + str(self._function_name)

class PromiseId:
    def __init__(self, id):
        self._id = id

    def __str__(self):
        return str(self._id)
    
class Promise:
    # raw print in gdb: print /r *(registry.registries[0]._M_ptr).promise_head._M_b._M_p
    
    def __init__(self, id: PromiseId, thread: Thread, source_location: SourceLocation, requester: Requester, state: State):
        self._id = id
        self._thread = thread
        self._source_location = source_location
        self._requester = requester
        self._state = state

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
        return not self._state.is_deleted()

    def __str__(self):
        return str(self._id) + ": waiter " + str(self._requester) + " " + str(self._source_location) + ", " + str(self._thread) + ", " + str(self._state) 

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
       self._promises = promises

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(Promise.from_gdb(promise) for promise in GdbAtomicList(value["promise_head"]["_M_b"]["_M_p"]))

    def promises(self):
        yield from self._promises
    
    def __str__(self):
        return "\n".join(str(promise) for promise in filter(lambda x: x.is_valid(), self._promises))
    
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
        self._thread_registries = registries

    @classmethod
    def from_gdb(cls, value: gdb.Value):
        return cls(ThreadRegistry.from_gdb(registry) for registry in GdbVector(value['registries']['_M_impl']))

    def promises(self) -> Iterable[MyTest]:
        for registry in self._thread_registries:
            for promise in registry.promises():
                yield promise


class AsyncRegistryPrinter:
    def __init__(self, value: gdb.Value):
        self._registry = AsyncRegistry.from_gdb(value)

    def to_string (self):
        return "async registry bla"
        # TODO forest = Forest(all_promises(ThreadRegistryList(self.thread_registries["_M_impl"])))

    def children(self):
        # TODO create stacktraces from the forest and yield each one
        counter = 0
        for promise in self._registry.promises():
            output = ("[%d]" % counter, str(promise))
            counter += 1
            yield output

def lookup_type (val):
    if str(val.type) == 'arangodb::async_registry::Registry':
        return AsyncRegistryPrinter(val)
    return None

gdb.pretty_printers.append (lookup_type)
