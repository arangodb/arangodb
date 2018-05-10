<!-- Integrate this into installation pages? -->

Filesystems
-----------

As one would expect for a database, we recommend a locally mounted filesystems.

NFS or similar network filesystems will not work.

On Linux we recommend the use of ext4fs, on Windows NTFS and on MacOS HFS+.

We recommend to **not** use BTRFS on Linux. It is known to not work well in conjunction with ArangoDB.
We experienced that ArangoDB faces latency issues on accessing its database files on BTRFS partitions.
In conjunction with BTRFS and AUFS we also saw data loss on restart.
