# Pretty Printer

Pretty print the stacktraces that result from the REST call _admin/async-registry

## Execute
Pipe the results of the REST call into the script, e.g.
```
curl -s http://localhost:8530/_admin/async-registry -u root: | ./pretty_printer.py
```
