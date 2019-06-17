#!/usr/bin/env bash
# limit memory useage for process
# using control groups https://en.wikipedia.org/wiki/Cgroups

# @author Jan Christoph Uhde

echo "usage: $0 <ram in mb> <swap in mb> command"

# get args
ram="$1"
swap="$2"
group=arango_mem
shift 2

# do not continue if anything goes wrong
set -e

# prepare control groups
sudo -- cgcreate -g "memory:/$group"

#ram
fpath="/sys/fs/cgroup/memory/$group/memory.limit_in_bytes"
sudo bash -c "echo $(( $ram * 1024 * 1024 )) > $fpath "

# swap
fpath="/sys/fs/cgroup/memory/$group/memory.memsw.limit_in_bytes"
if [[ -e $fpath ]]; then
    sudo bash -c " echo $(( $swap * 1024 * 1024 )) > $fpath"
fi

#execute
sudo cgexec -g memory:arango_mem su -l -p -c "$@" $USER

