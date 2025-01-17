"""Async Registry Pretty Printer for gdb

Parsing 

"""
from asyncregistry.promise_data import AsyncRegistry
# import gdb.printing

# TODO try to autoload

class AsyncRegistryPrinter:
    def __init__(self, value):
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

# def build_pretty_printer():
#     pp = gdb.printing.RegexpCollectionPrettyPrinter("asyncregistry")
#     pp.add_printer('arangodb::async_registry::Registry', "^arangodb::async_registry::Registry$", AsyncRegistryPrinter)
#     return pp

# def register_printer():
#     gdb.printing.register_pretty_printer(
#         gdb.current_objfile(),
#         build_pretty_printer())

