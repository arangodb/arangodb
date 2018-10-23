Arangosh
========

The ArangoDB shell (_arangosh_) is a command-line client tool that can be used
for administration of ArangoDB servers.

It offers a V8 JavaScript shell environment, in which you can use JS interfaces
and modules like the [`db` object](../../Appendix/References/DBObject.md) to
manage collections or run ad-hoc queries for instance, access the
[General Graph module](../../Graphs/GeneralGraphs/README.md) or other features.

It can be used as interactive shell (REPL) as well as to execute a JavaScript
string or file. It is not a general command line like PowerShell or Bash however.
Commands like `curl` or invocations of [ArangoDB programs and tools](../README.md)
are not possible inside of this JS shell!
