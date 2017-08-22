Operating System Configuration
==============================

File Systems
------------

(LINUX)

We recommend **not** to use BTRFS on linux, it's known to not work
well in conjunction with ArangoDB.  We experienced that arangodb
facing latency issues on accessing its database files on BTRFS
partitions.  In conjunction with BTRFS and AUFS we also saw data loss
on restart.


Virtual Memory Page Sizes
--------------------------

(LINUX)

By default, ArangoDB uses Jemalloc as the memory allocator. Jemalloc does a good
job of reducing virtual memory fragmentation, especially for long-running
processes. Unfortunately, some OS configurations can interfere with Jemalloc's
ability to function properly. Specifically, Linux's "transparent hugepages",
Windows' "large pages" and other similar features sometimes prevent Jemalloc
from returning unused memory to the operating system and result in unnecessarily
high memory use. Therefore, we recommend disabling these features when using
Jemalloc with ArangoDB. Please consult your operating system's documentation for
how to do this.

Execute

    sudo bash -c "echo madvise >/sys/kernel/mm/transparent_hugepage/enabled"
    sudo bash -c "echo madvise >/sys/kernel/mm/transparent_hugepage/defrag"

before executing `arangod`.

Swap Space
----------

(LINUX)

It is recommended to assign swap space for a server that is running arangod.
Configuring swap space can prevent the operating system's OOM killer from
killing ArangoDB too eagerly on Linux.

### Over-Commit Memory

Execute

    sudo bash -c "echo 0 >/proc/sys/vm/overcommit_memory"

before executing `arangod`.

From [www.kernel.org](https://www.kernel.org/doc/Documentation/sysctl/vm.txt):

- When this flag is 0, the kernel attempts to estimate the amount
  of free memory left when userspace requests more memory.

- When this flag is 1, the kernel pretends there is always enough
  memory until it actually runs out.

- When this flag is 2, the kernel uses a "never overcommit"
  policy that attempts to prevent any overcommit of memory.

### Zone Reclaim

Execute

    sudo bash -c "echo 0 >/proc/sys/vm/zone_reclaim_mode"

before executing `arangod`.

From [www.kernel.org](https://www.kernel.org/doc/Documentation/sysctl/vm.txt):

This is value ORed together of

- 1 = Zone reclaim on
- 2 = Zone reclaim writes dirty pages out
- 4 = Zone reclaim swaps pages

NUMA
----

Multi-processor systems often have non-uniform Access Memory (NUMA). ArangoDB
should be started with interleave on such system. This can be achieved using

    numactl --interleave=all arangod ...

Environment Variables
---------------------

It is recommended to set the environment variable `GLIBCXX_FORCE_NEW` to 1 on
systems that use glibc++ in order to disable the memory pooling built into
glibc++. That memory pooling is unnecessary because Jemalloc will already do
memory pooling.

Execute

    export GLIBCXX_FORCE_NEW=1


before starting `arangod`.

32bit
-----

While it is possible to compile ArangoDB on 32bit system, this is not a
recommended environment. 64bit systems can address a significantly bigger
memory region.

