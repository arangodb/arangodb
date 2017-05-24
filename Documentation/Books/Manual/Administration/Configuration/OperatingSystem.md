Operating System Configuration
==============================

Virtual Memory Page Sizes
--------------------------

By default, ArangoDB uses Jemalloc as the memory allocator. Jemalloc does a good
job of reducing virtual memory fragmentation, especially for long-running
processes. Unfortunately, some OS configurations can interfere with Jemalloc's
ability to function properly. Specifically, Linux's "transparent hugepages",
Windows' "large pages" and other similar features sometimes prevent Jemalloc
from returning unused memory to the operating system and result in unnecessarily
high memory use. Therefore, we recommend disabling these features when using
Jemalloc with ArangoDB. Please consult your operating system's documentation for
how to do this.


Swap Space
----------

It is recommended to assign swap space for a server that is running arangod.
Configuring swap space can prevent the operating system's OOM killer from
killing ArangoDB too eagerly on Linux.


NUMA
----


Environment Variables
---------------------

It is recommended to set the environment variable `GLIBCXX_FORCE_NEW` to 1 on
systems that use glibc++ in order to disable the memory pooling built into
glibc++. That memory pooling is unnecessary because Jemalloc will already do
memory pooling.
