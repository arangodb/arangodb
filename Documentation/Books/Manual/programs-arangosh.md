---
layout: default
description: The ArangoDB shell (arangosh) is a command-line client tool that can be usedfor administration of ArangoDB servers
---
Arangosh
========

The ArangoDB shell (_arangosh_) is a command-line client tool that can be used
for administration of ArangoDB servers.

It offers a V8 JavaScript shell environment, in which you can use JS interfaces
and modules like the [`db` object](appendix-references-dbobject.html) to
manage collections or run ad-hoc queries for instance, access the
[General Graph module](graphs-general-graphs.html) or other features.

It can be used as interactive shell (REPL) as well as to execute a JavaScript
string or file. It is not a general command line like PowerShell or Bash however.
Commands like `curl` or invocations of [ArangoDB programs and tools](programs.html)
are not possible inside of this JS shell!
