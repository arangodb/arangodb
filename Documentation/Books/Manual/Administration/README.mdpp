Administration
==============

Most administration can be managed using the *arangosh*.


Filesystems
===========
As one would expect for a database, we recommend a localy mounted filesystems.

NFS or similar network filesystems will not work.

On Linux we recommend the use of ext4fs, on Windows NTFS and on MacOS HFS+.

We recommend **not** to use BTRFS on linux, it's known to not work well in conjunction with ArangoDB.
We experienced that arangodb facing latency issues on accessing its database files on BTRFS partitions.
In conjunction with BTRFS and AUFS we also saw data loss on restart.
