---
layout: default
description: The tool is to be used with caution, under guidance of ArangoDB support!
---
Arango-dfdb
===========

{% hint 'danger' %}
The tool is to be used with caution, under guidance of ArangoDB support!
{% endhint %}

The ArangoDB Datafile Debugger can check datafiles for corruptions
and remove invalid entries to repair them. Such corruptions should
not occur unless there was a hardware failure.

The options are the same as for [arangod](programs-arangod-options.html).

{% hint 'info' %}
`arango-dfdb` works with the
[MMFiles storage engine](architecture-storage-engines.html) only.
{% endhint %}
