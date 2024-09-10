
# LLVM Symbolizer client/server infrastructure
When ran with TSAN/ALUBSAN etc. processes fork another process to resolve their symbols from addresses. These sub-processes are quite memory hungry depending on build model. ArangoDB cluster tests launch about half a dozen processes, and if each of those starts it's own symbolizer, then these add up to a larger amount effectively draining the host out of resources during CI-Runs or even local runs on bigger machines.
However, these forked sub processes all work on similar if not the same binaries, hence could we only have *one* of these resource hoggers instead of many?
Along comes the symbolizer server and client solution - *one* symbolizer is spawned by the server process, communicating via pipes. This server then runs a simple Python HTTP server to process POST requests from the clients:

```
./utils/llvm-symbolizer-server.py
```

Now only all the other processes need to be told that they should only launch a tiny python client (`llvm-symbolizer-client.py`) instead of running their own symbolizer by the respective environment variables. These client processes implement the same stdin/stdout protocol as the llvm-symbolizer, but then forward the request via HTTP to the server process:
export TSAN_OPTIONS="external_symbolizer_path=$(pwd)/utils/llvm-symbolizer-client.py"
```

The resulting python processes will not be resource intense in comparison to the instrumented processes, hence no further optimisation like implementing it in GO were untertaken.
