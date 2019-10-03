---
layout: default
description: Arangorestore is a command-line client tool to restore backups created byArangodump toArangoDB servers
---
Arangorestore
=============

_Arangorestore_ is a command-line client tool to restore backups created by
[_Arangodump_](programs-arangodump.html) to
[ArangoDB servers](programs-arangod.html).

If you want to import data in formats like JSON or CSV, see
[_Arangoimport_](programs-arangoimport.html) instead.

_Arangorestore_ can restore selected collections or all collections of a backup,
optionally including _system_ collections. One can restore the structure, i.e.
the collections with their configuration with or without data.
Views can also be dumped or restored (either all of them or selectively).

{% hint 'tip' %}
In order to speed up the _arangorestore_ performance in a Cluster environment,
the [Fast Cluster Restore](programs-arangorestore-fast-cluster-restore.html)
procedure is recommended.
{% endhint %}
