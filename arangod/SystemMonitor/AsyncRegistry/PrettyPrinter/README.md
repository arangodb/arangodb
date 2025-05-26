# Pretty Printer for ArangoDB's async registry

ArangoDB's async registry includes all currently active asynchronous operations and their dependencies.

This python-package includes one package that pretty-prints the async registry inside a gdb debugging session and one script that improves the readability the async registry items coming from a REST call. Both applications print all active async operations in form of stacktraces with one operation per line and use some ascii-art to improve the readability.

## Pretty printing in gdb

When debugging, it can be helful to get an overview of all active async operations at the current execution state. The standard version is almost impossible to read due to the complexity of the registry. To use the async-registry pretty-printer in gdb you need to 
1. Start gdb from inside the arangodb directory
2. give the `.gdbinit` file in this directory to gdb at startup, e.g. via:
```sh
gdb -ix <path to the .gdbinit file in this directory> <executable via --args or pid via -p>
```
This ensures that the pretty-printer is loaded to gdb. You can check if it is loaded inside gdb with `info pretty-print` which should include `async_registry`. Now, whenever you print the global registry `arangodb::async_registry::registry`, gdb will show you the pretty-print version.

### Run tests

#### Unit tests

```
ctest --build -R async_registry_python_pretty_printer_test
```

#### Integration tests using gdb

```
cmake --build --target async_registry_gdb_pretty_printer
ctest --build -R async_registry_gdb_pretty_printer_test
```
use `-V` option on `ctest` to see gdb debug output

## Pretty printing the REST call

ArangoDB provides a REST call which gives you all active async operations. The pretty-printer python script improves the readability of the output. Use it as the following:

```sh
curl -s <server>/_admin/async-registry -u root: | ./src/pretty-printer.py
```
