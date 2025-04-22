"""Async Registry Pretty Printer for gdb

Parsing 

"""
from asyncregistry.gdb_data import AsyncRegistry, Promise
from asyncregistry.gdb_forest import Forest
from asyncregistry.stacktrace import Stacktrace

class AsyncRegistryPrinter:
    def __init__(self, value):
        self._registry = AsyncRegistry.from_gdb(value)

    def to_string (self):
        self.forest = Forest.from_promises(map(lambda promise: (promise.id, promise.requester.content, promise), filter(lambda promise: promise.is_valid(), self._registry.promises())))
        return "async registry"

    def children(self):
        '''One child is one stacktrace per thread'''
        current_stacktrace = Stacktrace()
        for (hierarchy, promise_id, data) in self.forest:
            stacktrace = current_stacktrace.append(hierarchy, data)
            if stacktrace != None:
                current_stacktrace = Stacktrace()
                # there is always a synchronous thread waiting for the root async observable
                waiting_thread = data.requester.content
                stacktrace.append(("─ ", str(waiting_thread)))
                yield ("\n[%s]" % str(waiting_thread), "\n" + "\n".join(ascii + str(promise) for (ascii, promise) in stacktrace))

def async_registry (val):
    if str(val.type) == 'arangodb::async_registry::Registry':
        return AsyncRegistryPrinter(val)
    return None

gdb.pretty_printers.append (async_registry)
